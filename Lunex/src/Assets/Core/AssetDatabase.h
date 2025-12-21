#pragma once

/**
 * @file AssetDatabase.h
 * @brief Asset database for project scanning and metadata
 * 
 * Part of the Unified Assets System
 */

#include "Asset.h"

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

namespace Lunex {

	// ============================================================================
	// ASSET DATABASE ENTRY
	// ============================================================================
	struct AssetDatabaseEntry {
		UUID AssetID;
		std::filesystem::path RelativePath;
		AssetType Type;
		std::string Name;
		size_t FileSize = 0;
		std::filesystem::file_time_type LastModified;
		
		std::vector<UUID> Dependencies;
		
		bool HasThumbnail = false;
		std::filesystem::path ThumbnailPath;
		
		std::unordered_map<std::string, std::string> CustomMetadata;
	};

	// ============================================================================
	// ASSET DATABASE
	// ============================================================================
	class AssetDatabase {
	public:
		static AssetDatabase& Get() {
			static AssetDatabase instance;
			return instance;
		}
		
		AssetDatabase() = default;
		~AssetDatabase() = default;

		// Initialization
		void Initialize() { m_IsInitialized = true; }
		void Shutdown() { m_IsInitialized = false; }
		void Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& assetsFolder);
		
		void ScanAssets();
		bool SaveDatabase();
		bool LoadDatabase();
		
		// Registration
		void RegisterAsset(const AssetDatabaseEntry& entry);
		void UnregisterAsset(UUID assetID);
		void UpdateAsset(UUID assetID, const AssetDatabaseEntry& entry);
		
		// Queries
		const AssetDatabaseEntry* GetAssetEntry(UUID assetID) const;
		const AssetDatabaseEntry* GetAssetEntryByPath(const std::filesystem::path& path) const;
		std::vector<AssetDatabaseEntry> GetAssetsByType(AssetType type) const;
		const std::unordered_map<UUID, AssetDatabaseEntry>& GetAllAssets() const { return m_Assets; }
		std::vector<UUID> GetDependencies(UUID assetID) const;
		std::vector<UUID> GetDependents(UUID assetID) const;
		
		// File watching
		void UpdateFileWatchers();
		void SetAssetModifiedCallback(std::function<void(UUID, const std::filesystem::path&)> callback) {
			m_OnAssetModified = callback;
		}
		
		// Utilities
		static AssetType GetAssetTypeFromExtension(const std::string& extension);
		static std::string GetExtensionForAssetType(AssetType type);
		std::filesystem::path GetRelativePath(const std::filesystem::path& absolutePath) const;
		std::filesystem::path GetAbsolutePath(const std::filesystem::path& relativePath) const;
		
		// Statistics
		size_t GetAssetCount() const { return m_Assets.size(); }
		size_t GetAssetCountByType(AssetType type) const;
		
	private:
		void ScanDirectory(const std::filesystem::path& directory);
		AssetDatabaseEntry ExtractAssetMetadata(const std::filesystem::path& filePath);
		std::vector<UUID> ExtractDependencies(const std::filesystem::path& filePath, AssetType type);
		UUID GenerateAssetID(const std::filesystem::path& filePath);
		
	private:
		std::filesystem::path m_ProjectRoot;
		std::filesystem::path m_AssetsFolder;
		std::filesystem::path m_DatabasePath;
		
		std::unordered_map<UUID, AssetDatabaseEntry> m_Assets;
		std::unordered_map<std::string, UUID> m_PathToID;
		
		std::function<void(UUID, const std::filesystem::path&)> m_OnAssetModified;
		
		mutable std::mutex m_Mutex;
		bool m_IsInitialized = false;
	};

}
