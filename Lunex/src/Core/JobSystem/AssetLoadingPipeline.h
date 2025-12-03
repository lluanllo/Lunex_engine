#pragma once

#include "Core/Core.h"
#include "JobSystem.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <any>

// Forward declarations
namespace Lunex {
	class Texture2D;
	class Mesh;
}

namespace Lunex {

	/// <summary>
	/// Asset type enumeration for loading pipeline.
	/// </summary>
	enum class AssetType {
		Texture,
		Mesh,
		Material,
		Scene,
		Audio,
		Shader,
		Unknown
	};

	/// <summary>
	/// Three-phase async asset loading pipeline:
	/// 1. IO Phase (IO thread): Read file bytes from disk
	/// 2. Parse Phase (worker thread): Deserialize/parse data
	/// 3. Upload Phase (main thread): GPU upload
	/// 
	/// Pipeline ensures:
	/// - No disk I/O on main thread (prevents stuttering)
	/// - No heavy parsing on main thread (prevents frame drops)
	/// - GPU upload only on main thread (thread-safe)
	/// - Automatic cancellation if scene reloads (version tokens)
	/// 
	/// Usage Example:
	/// ```cpp
	/// AssetLoadingPipeline::LoadRequest request;
	/// request.FilePath = "assets/textures/wall.png";
	/// request.Type = AssetType::Texture;
	/// request.SceneVersion = scene->GetVersion();
	/// request.OnComplete = [](std::any asset) {
	///     auto texture = std::any_cast<Ref<Texture2D>>(asset);
	///     if (texture) {
	///         LNX_LOG_INFO("Texture loaded: {0}", texture->GetPath());
	///     }
	/// };
	/// AssetLoadingPipeline::Get().LoadAssetAsync(request);
	/// ```
	/// </summary>
	class AssetLoadingPipeline {
	public:
		/// <summary>
		/// Async asset load request.
		/// </summary>
		struct LoadRequest {
			/// File path to load
			std::filesystem::path FilePath;

			/// Asset type hint (for parser selection)
			AssetType Type = AssetType::Unknown;

			/// Completion callback (called on main thread) - receives std::any containing the asset
			std::function<void(std::any)> OnComplete;

			/// Scene version for cancellation
			uint64_t SceneVersion = 0;

			/// Priority for scheduling
			JobPriority Priority = JobPriority::Normal;
		};

		AssetLoadingPipeline() = default;
		~AssetLoadingPipeline() = default;

		// Singleton access
		static AssetLoadingPipeline& Get();

		/// <summary>
		/// Load asset asynchronously using 3-phase pipeline.
		/// </summary>
		/// <param name="request">Load request parameters</param>
		void LoadAssetAsync(const LoadRequest& request);

		/// <summary>
		/// Load texture asynchronously (convenience method).
		/// </summary>
		/// <param name="path">Texture file path</param>
		/// <param name="onComplete">Completion callback</param>
		/// <param name="sceneVersion">Scene version for cancellation</param>
		void LoadTextureAsync(
			const std::filesystem::path& path,
			std::function<void(Ref<Texture2D>)> onComplete,
			uint64_t sceneVersion = 0
		);

		/// <summary>
		/// Load mesh asynchronously (convenience method).
		/// </summary>
		/// <param name="path">Mesh file path</param>
		/// <param name="onComplete">Completion callback</param>
		/// <param name="sceneVersion">Scene version for cancellation</param>
		void LoadMeshAsync(
			const std::filesystem::path& path,
			std::function<void(Ref<Mesh>)> onComplete,
			uint64_t sceneVersion = 0
		);

		/// <summary>
		/// Get number of pending asset loads.
		/// </summary>
		uint32_t GetPendingLoadCount() const {
			return m_PendingLoads.load(std::memory_order_relaxed);
		}

	private:
		/// <summary>
		/// Phase 1: Read file bytes from disk (IO thread).
		/// </summary>
		/// <param name="request">Load request</param>
		/// <returns>Raw file bytes</returns>
		static std::vector<uint8_t> PhaseIO(const LoadRequest& request);

		/// <summary>
		/// Phase 2: Parse/deserialize data (worker thread).
		/// </summary>
		/// <param name="request">Load request</param>
		/// <param name="bytes">Raw file bytes</param>
		/// <returns>Parsed asset data (CPU-side) wrapped in std::any</returns>
		static std::any PhaseParse(const LoadRequest& request, std::vector<uint8_t> bytes);

		/// <summary>
		/// Phase 3: Upload to GPU (main thread via command).
		/// </summary>
		/// <param name="request">Load request</param>
		/// <param name="asset">Parsed asset wrapped in std::any</param>
		static void PhaseUpload(const LoadRequest& request, std::any asset);

		/// <summary>
		/// Detect asset type from file extension.
		/// </summary>
		static AssetType DetectAssetType(const std::filesystem::path& path);

	private:
		// Pending load counter
		std::atomic<uint32_t> m_PendingLoads{ 0 };

		// Singleton instance
		static AssetLoadingPipeline s_Instance;
	};

} // namespace Lunex
