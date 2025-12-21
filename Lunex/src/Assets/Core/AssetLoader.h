#pragma once

/**
 * @file AssetLoader.h
 * @brief Async asset loading with JobSystem
 * 
 * Unified Assets Architecture:
 * - Parallel asset loading using JobSystem
 * - Dependency resolution
 * - Progress tracking
 * - Batch loading support
 */

#include "Asset.h"
#include "AssetRegistry.h"

#include <functional>
#include <vector>
#include <atomic>
#include <mutex>

namespace Lunex {

	// Forward declare JobSystem
	class JobSystem;

	/**
	 * @struct AssetLoadJob
	 * @brief Job for loading a single asset
	 */
	struct AssetLoadJob {
		UUID AssetID;
		std::filesystem::path Path;
		AssetType Type;
		std::function<void(Ref<Asset>)> Callback;
		std::vector<UUID> Dependencies;
		
		std::atomic<bool> IsComplete{false};
		Ref<Asset> LoadedAsset;
	};

	/**
	 * @class AssetLoader
	 * @brief Handles asynchronous asset loading
	 */
	class AssetLoader {
	public:
		static AssetLoader& Get();
		
		void Initialize();
		void Shutdown();
		
		// ========== ASYNC LOADING ==========
		
		/**
		 * @brief Load asset asynchronously
		 * @param path Asset file path
		 * @param callback Called when asset is loaded (on main thread)
		 */
		template<typename T>
		void LoadAsync(const std::filesystem::path& path, std::function<void(Ref<T>)> callback);
		
		/**
		 * @brief Load multiple assets in parallel
		 */
		template<typename T>
		void LoadBatchAsync(const std::vector<std::filesystem::path>& paths, 
							std::function<void(std::vector<Ref<T>>)> callback);
		
		/**
		 * @brief Cancel pending load operation
		 */
		void CancelLoad(UUID assetID);
		
		/**
		 * @brief Wait for all pending loads to complete
		 */
		void WaitForAll();
		
		// ========== PROGRESS TRACKING ==========
		
		size_t GetPendingCount() const;
		size_t GetCompletedCount() const;
		float GetProgress() const;
		
		// ========== UPDATE (Call from main thread) ==========
		
		/**
		 * @brief Process completed loads and invoke callbacks
		 * Call this every frame from main thread
		 */
		void Update();
		
	private:
		AssetLoader() = default;
		~AssetLoader() = default;
		
		AssetLoader(const AssetLoader&) = delete;
		AssetLoader& operator=(const AssetLoader&) = delete;
		
		// Internal load function (runs on worker thread)
		Ref<Asset> LoadAssetInternal(const std::filesystem::path& path, AssetType type);
		
	private:
		std::vector<Ref<AssetLoadJob>> m_PendingJobs;
		std::vector<Ref<AssetLoadJob>> m_CompletedJobs;
		
		mutable std::mutex m_JobsMutex;
		bool m_Initialized = false;
	};

	// ============================================================================
	// TEMPLATE IMPLEMENTATIONS
	// ============================================================================

	template<typename T>
	void AssetLoader::LoadAsync(const std::filesystem::path& path, std::function<void(Ref<T>)> callback) {
		if (!m_Initialized) {
			// Fallback to sync loading
			if (callback) {
				auto asset = T::LoadFromFile(path);
				callback(asset);
			}
			return;
		}
		
		// Create load job
		auto job = CreateRef<AssetLoadJob>();
		job->Path = path;
		job->Type = T::StaticType();
		job->AssetID = UUID();
		
		// Wrapper callback
		job->Callback = [callback](Ref<Asset> asset) {
			if (callback) {
				callback(std::dynamic_pointer_cast<T>(asset));
			}
		};
		
		{
			std::lock_guard<std::mutex> lock(m_JobsMutex);
			m_PendingJobs.push_back(job);
		}
		
		// Submit to thread pool (simplified - uses std::async)
		std::thread([this, job]() {
			job->LoadedAsset = LoadAssetInternal(job->Path, job->Type);
			job->IsComplete = true;
			
			std::lock_guard<std::mutex> lock(m_JobsMutex);
			m_CompletedJobs.push_back(job);
		}).detach();
	}

	template<typename T>
	void AssetLoader::LoadBatchAsync(const std::vector<std::filesystem::path>& paths, 
									 std::function<void(std::vector<Ref<T>>)> callback) {
		if (!m_Initialized || paths.empty()) {
			if (callback) callback({});
			return;
		}
		
		auto results = std::make_shared<std::vector<Ref<T>>>(paths.size());
		auto counter = std::make_shared<std::atomic<size_t>>(0);
		
		for (size_t i = 0; i < paths.size(); ++i) {
			LoadAsync<T>(paths[i], [results, counter, i, callback, total = paths.size()](Ref<T> asset) {
				(*results)[i] = asset;
				
				if (++(*counter) == total) {
					if (callback) {
						callback(*results);
					}
				}
			});
		}
	}

} // namespace Lunex
