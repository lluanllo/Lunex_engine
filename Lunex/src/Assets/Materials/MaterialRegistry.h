#pragma once

/**
 * @file MaterialRegistry.h
 * @brief Material caching and management system
 * 
 * AAA Architecture: MaterialRegistry lives in Assets/Materials/
 * Manages material loading, caching, and hot-reload.
 */

#include "Core/Core.h"
#include "Core/UUID.h"
#include "MaterialAsset.h"
#include "Resources/Render/MaterialInstance.h"

#include <unordered_map>
#include <filesystem>
#include <string>

namespace Lunex {

	/**
	 * @class MaterialRegistry
	 * @brief Centralized material management system
	 * 
	 * Provides:
	 * - Material asset caching
	 * - Hot-reload when .lumat files change
	 * - Default material creation
	 * - Fast lookup by UUID or Path
	 */
	class MaterialRegistry {
	public:
		static MaterialRegistry& Get() {
			static MaterialRegistry instance;
			return instance;
		}

		MaterialRegistry(const MaterialRegistry&) = delete;
		MaterialRegistry& operator=(const MaterialRegistry&) = delete;

		// ========== ASSET MANAGEMENT ==========

		Ref<MaterialAsset> LoadMaterial(const std::filesystem::path& path);
		void RegisterMaterial(Ref<MaterialAsset> material);
		void UnregisterMaterial(UUID id);
		Ref<MaterialAsset> GetMaterial(UUID id) const;
		Ref<MaterialAsset> GetMaterialByPath(const std::filesystem::path& path) const;
		bool IsMaterialLoaded(UUID id) const;
		bool IsMaterialLoadedByPath(const std::filesystem::path& path) const;

		// ========== DEFAULT MATERIALS ==========

		Ref<MaterialAsset> GetDefaultMaterial();
		Ref<MaterialAsset> CreateNewMaterial(const std::string& name = "New Material");

		// ========== HOT-RELOAD ==========

		void ReloadMaterial(UUID id);
		void ReloadMaterialByPath(const std::filesystem::path& path);
		void ReloadModifiedMaterials();
		void Update();

		// ========== SEARCH AND LISTING ==========

		std::vector<Ref<MaterialAsset>> GetAllMaterials() const;
		std::vector<Ref<MaterialAsset>> SearchMaterialsByName(const std::string& query) const;
		size_t GetLoadedMaterialCount() const { return m_MaterialCache.size(); }

		// ========== CLEANUP ==========

		void ClearUnusedMaterials();
		void ClearAll();

	private:
		MaterialRegistry();
		~MaterialRegistry() = default;

		std::unordered_map<UUID, Ref<MaterialAsset>> m_MaterialCache;
		std::unordered_map<std::string, UUID> m_PathToUUID;
		Ref<MaterialAsset> m_DefaultMaterial;

		struct FileWatchData {
			std::filesystem::path Path;
			std::filesystem::file_time_type LastModified;
			UUID MaterialID;
		};

		std::unordered_map<std::string, FileWatchData> m_FileWatchers;

		void AddFileWatcher(const std::filesystem::path& path, UUID materialID);
		void RemoveFileWatcher(const std::filesystem::path& path);
		void UpdateFileTimestamps();
		Ref<MaterialAsset> CreateDefaultMaterial();
		std::string NormalizePath(const std::filesystem::path& path) const;
	};

} // namespace Lunex
