#pragma once

/**
 * @file AssetCore.h  
 * @brief Central include for Unified Asset System
 * 
 * Unified Assets Architecture:
 * - All asset-related code in Assets/ folder
 * - Core types and registry in Assets/Core/
 * - Type-specific assets in Assets/<Type>/
 * - JobSystem integration for async loading
 */

#include "Asset.h"
#include "AssetRegistry.h"
#include "AssetDatabase.h"
#include "AssetHandle.h"
#include "AssetLoader.h"

namespace Lunex {

	/**
	 * @class AssetManager
	 * @brief High-level asset management facade
	 * 
	 * Unified interface for:
	 * - Synchronous and asynchronous asset loading
	 * - Asset caching and hot-reload
	 * - JobSystem integration for parallel loading
	 */
	class AssetManager {
	public:
		static void Initialize();
		static void Shutdown();
		
		// Synchronous loading
		template<typename T>
		static Ref<T> Load(const std::filesystem::path& path) {
			return AssetRegistry::Get().Load<T>(path);
		}
		
		// Asynchronous loading with JobSystem
		template<typename T>
		static void LoadAsync(const std::filesystem::path& path, std::function<void(Ref<T>)> callback) {
			AssetLoader::Get().LoadAsync<T>(path, callback);
		}
		
		// Get loaded asset
		template<typename T>
		static Ref<T> Get(UUID id) {
			return AssetRegistry::Get().Get<T>(id);
		}
		
		// Asset registry access
		static AssetRegistry& GetRegistry() { return AssetRegistry::Get(); }
		static AssetDatabase& GetDatabase() { return AssetDatabase::Get(); }
		
		// Update (call every frame for hot-reload and async callbacks)
		static void Update(float deltaTime) {
			AssetRegistry::Get().Update(deltaTime);
			AssetLoader::Get().Update();
		}
		
	private:
		AssetManager() = delete;
	};

} // namespace Lunex
