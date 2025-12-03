#include "stpch.h"
#include "AssetLoadingPipeline.h"
#include "Log/Log.h"
#include "Renderer/Texture.h"
#include <fstream>
#include <chrono>

namespace Lunex {

	// Singleton instance
	AssetLoadingPipeline AssetLoadingPipeline::s_Instance;

	AssetLoadingPipeline& AssetLoadingPipeline::Get() {
		return s_Instance;
	}

	// ========================================================================
	// ASYNC LOADING
	// ========================================================================

	void AssetLoadingPipeline::LoadAssetAsync(const LoadRequest& request) {
		LNX_CORE_ASSERT(!request.FilePath.empty(), "Asset path cannot be empty!");
		LNX_CORE_ASSERT(request.OnComplete, "OnComplete callback cannot be null!");

		m_PendingLoads.fetch_add(1, std::memory_order_relaxed);

		// Create counter for pipeline phases
		auto counter = JobSystem::Get().CreateCounter(3); // IO + Parse + Upload

		// Phase 1: IO (on IO worker thread)
		JobSystem::Get().Schedule(
			[request, counter]() {
				auto bytes = PhaseIO(request);

				if (bytes.empty()) {
					LNX_LOG_ERROR("Failed to load asset file: {0}", request.FilePath.string());
					counter->Decrement(); // IO failed
					counter->Decrement(); // Skip Parse
					counter->Decrement(); // Skip Upload
					AssetLoadingPipeline::Get().m_PendingLoads.fetch_sub(1, std::memory_order_relaxed);
					return;
				}

				counter->Decrement(); // IO completed

				// Phase 2: Parse (on worker thread)
				JobSystem::Get().Schedule(
					[request, bytes = std::move(bytes), counter]() {
						auto asset = PhaseParse(request, bytes); // Don't use move here, bytes is const in lambda

						if (!asset.has_value()) {
							LNX_LOG_ERROR("Failed to parse asset: {0}", request.FilePath.string());
							counter->Decrement(); // Parse failed
							counter->Decrement(); // Skip Upload
							AssetLoadingPipeline::Get().m_PendingLoads.fetch_sub(1, std::memory_order_relaxed);
							return;
						}

						counter->Decrement(); // Parse completed

						// Phase 3: Upload (on main thread via command)
						// Create a struct to hold the std::any asset
						struct AssetHolder {
							std::any AssetData;
							LoadRequest Request;
							Ref<JobCounter> Counter;
						};

						auto holder = std::make_unique<AssetHolder>();
						holder->AssetData = std::move(asset);
						holder->Request = request;
						holder->Counter = counter;

						auto cmd = Command::CreateWithOwnership<AssetHolder>(
							request.SceneVersion,
							std::move(holder),
							[](MainThreadContext& ctx, AssetHolder* h) {
								PhaseUpload(h->Request, h->AssetData);
								h->Counter->Decrement(); // Upload completed

								// Call completion callback
								if (h->Request.OnComplete) {
									h->Request.OnComplete(h->AssetData);
								}

								AssetLoadingPipeline::Get().m_PendingLoads.fetch_sub(1, std::memory_order_relaxed);
							}
						);

						JobSystem::Get().PushMainThreadCommand(std::move(cmd));
					},
					nullptr,
					nullptr,
					request.Priority,
					request.SceneVersion
				);
			},
			nullptr,
			nullptr,
			JobPriority::High, // IO jobs are high priority
		 request.SceneVersion
		);
	}

	// ========================================================================
	// CONVENIENCE METHODS
	// ========================================================================

	void AssetLoadingPipeline::LoadTextureAsync(
		const std::filesystem::path& path,
		std::function<void(Ref<Texture2D>)> onComplete,
		uint64_t sceneVersion
	) {
		LoadRequest request;
		request.FilePath = path;
		request.Type = AssetType::Texture;
		request.SceneVersion = sceneVersion;
		request.OnComplete = [onComplete](std::any asset) {
			if (onComplete && asset.has_value()) {
				try {
					auto texture = std::any_cast<Ref<Texture2D>>(asset);
					onComplete(texture);
				}
				catch (const std::bad_any_cast& e) {
					LNX_LOG_ERROR("Failed to cast asset to Texture2D: {0}", e.what());
				}
			}
		};

		LoadAssetAsync(request);
	}

	void AssetLoadingPipeline::LoadMeshAsync(
		const std::filesystem::path& path,
		std::function<void(Ref<Mesh>)> onComplete,
		uint64_t sceneVersion
	) {
		LoadRequest request;
		request.FilePath = path;
		request.Type = AssetType::Mesh;
		request.SceneVersion = sceneVersion;
		request.OnComplete = [onComplete](std::any asset) {
			if (onComplete && asset.has_value()) {
				try {
					auto mesh = std::any_cast<Ref<Mesh>>(asset);
					onComplete(mesh);
				}
				catch (const std::bad_any_cast& e) {
					LNX_LOG_ERROR("Failed to cast asset to Mesh: {0}", e.what());
				}
			}
		};

		LoadAssetAsync(request);
	}

	// ========================================================================
	// PHASE IMPLEMENTATIONS
	// ========================================================================

	std::vector<uint8_t> AssetLoadingPipeline::PhaseIO(const LoadRequest& request) {
		std::vector<uint8_t> bytes;

		try {
			auto startTime = std::chrono::high_resolution_clock::now();

			// Open file in binary mode
			std::ifstream file(request.FilePath, std::ios::binary | std::ios::ate);
			if (!file.is_open()) {
				LNX_LOG_ERROR("Failed to open file: {0}", request.FilePath.string());
				return bytes;
			}

			// Get file size
			std::streamsize fileSize = file.tellg();
			file.seekg(0, std::ios::beg);

			// Read entire file
			bytes.resize(static_cast<size_t>(fileSize));
			if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize)) {
				LNX_LOG_ERROR("Failed to read file: {0}", request.FilePath.string());
				bytes.clear();
				return bytes;
			}

			auto endTime = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

			LNX_LOG_TRACE("Loaded {0} bytes from {1} in {2}ms",
				bytes.size(), request.FilePath.filename().string(), duration);
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Exception during file read: {0}", e.what());
			bytes.clear();
		}

		return bytes;
	}

	std::any AssetLoadingPipeline::PhaseParse(const LoadRequest& request, std::vector<uint8_t> bytes) {
		// Detect asset type if not specified
		AssetType type = request.Type;
		if (type == AssetType::Unknown) {
			type = DetectAssetType(request.FilePath);
		}

		try {
			auto startTime = std::chrono::high_resolution_clock::now();

			std::any asset;

			switch (type) {
			case AssetType::Texture: {
				// Parse texture using stb_image (or similar)
				// NOTE: Actual implementation would use Texture2D::CreateFromMemory()
				// For now, we'll create a placeholder
				auto texture = Texture2D::Create(request.FilePath.string());
				asset = texture;
				break;
			}

			case AssetType::Mesh: {
				// Parse mesh using Assimp (or similar)
				// NOTE: Actual implementation would use Mesh::LoadFromMemory()
				// For now, log a message
				LNX_LOG_WARN("Mesh parsing not yet implemented in AssetLoadingPipeline");
				return std::any(); // Empty
			}

			case AssetType::Material: {
				// Parse material from YAML
				LNX_LOG_WARN("Material parsing not yet implemented in AssetLoadingPipeline");
				return std::any(); // Empty
			}

			case AssetType::Scene: {
				// Parse scene from YAML
				LNX_LOG_WARN("Scene parsing not yet implemented in AssetLoadingPipeline");
				return std::any(); // Empty
			}

			default:
				LNX_LOG_ERROR("Unsupported asset type for: {0}", request.FilePath.string());
				return std::any(); // Empty
			}

			auto endTime = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

			LNX_LOG_TRACE("Parsed asset {0} in {1}ms",
				request.FilePath.filename().string(), duration);

			return asset;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Exception during asset parsing: {0}", e.what());
			return std::any(); // Empty
		}
	}

	void AssetLoadingPipeline::PhaseUpload(const LoadRequest& request, std::any asset) {
		if (!asset.has_value()) {
			LNX_LOG_ERROR("Cannot upload null asset");
			return;
		}

		try {
			auto startTime = std::chrono::high_resolution_clock::now();

			// Asset-specific GPU upload
			// NOTE: Actual implementation would call asset->UploadToGPU()
			// For now, just log
			LNX_LOG_TRACE("GPU upload for asset: {0}", request.FilePath.filename().string());

			auto endTime = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

			LNX_LOG_TRACE("Uploaded asset {0} to GPU in {1}ms",
				request.FilePath.filename().string(), duration);
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Exception during GPU upload: {0}", e.what());
		}
	}

	// ========================================================================
	// UTILITY
	// ========================================================================

	AssetType AssetLoadingPipeline::DetectAssetType(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
			ext == ".bmp" || ext == ".tga" || ext == ".hdr") {
			return AssetType::Texture;
		}
		else if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" ||
			ext == ".glb" || ext == ".dae") {
			return AssetType::Mesh;
		}
		else if (ext == ".lumat") {
			return AssetType::Material;
		}
		else if (ext == ".lunex") {
			return AssetType::Scene;
		}
		else if (ext == ".glsl" || ext == ".vert" || ext == ".frag") {
			return AssetType::Shader;
		}
		else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
			return AssetType::Audio;
		}

		return AssetType::Unknown;
	}

} // namespace Lunex
