#pragma once

#include "Core/Core.h"
#include "Core/UUID.h"
#include "Asset.h"

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

namespace Lunex {

	// ============================================================================
	// ASSET DATABASE ENTRY
	// Stores metadata about an asset in the project
	// ============================================================================
	struct AssetDatabaseEntry {
		UUID AssetID;
		std::filesystem::path RelativePath;
		AssetType Type;
		std::string Name;
		size_t FileSize = 0;
		std::filesystem::file_time_type LastModified;
		
		// Dependencies (other assets this asset references)
		std::vector<UUID> Dependencies;
		
		// Thumbnail metadata
		bool HasThumbnail = false;
		std::filesystem::path ThumbnailPath;
		
		// Custom metadata per asset type
		std::unordered_map<std::string, std::string> CustomMetadata;
	};

	// ============================================================================
	// ASSET DATABASE
	// Centralized database of all assets in the project
	// Stored as .lnxast file in project root
	// ============================================================================
	class AssetDatabase {
	public:
		AssetDatabase() = default;
		~AssetDatabase() = default;

		// ========== INITIALIZATION ==========
		
		// Initialize database and scan project folder
		void Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& assetsFolder);
		
		// Scan assets folder and update database
		void ScanAssets();
		
		// Save database to .lnxast file
		bool SaveDatabase();
		
		// Load database from .lnxast file
		bool LoadDatabase();
		
		// ========== ASSET REGISTRATION ==========
		
		// Register an asset in the database
		void RegisterAsset(const AssetDatabaseEntry& entry);
		
		// Unregister an asset
		void UnregisterAsset(UUID assetID);
		
		// Update asset metadata
		void UpdateAsset(UUID assetID, const AssetDatabaseEntry& entry);
		
		// ========== QUERIES ==========
		
		// Get asset entry by ID
		const AssetDatabaseEntry* GetAssetEntry(UUID assetID) const;
		
		// Get asset entry by path
		const AssetDatabaseEntry* GetAssetEntryByPath(const std::filesystem::path& path) const;
		
		// Get all assets of a specific type
		std::vector<AssetDatabaseEntry> GetAssetsByType(AssetType type) const;
		
		// Get all assets
		const std::unordered_map<UUID, AssetDatabaseEntry>& GetAllAssets() const { return m_Assets; }
		
		// Find dependencies of an asset (assets that this asset references)
		std::vector<UUID> GetDependencies(UUID assetID) const;
		
		// Find dependents (assets that reference this asset)
		std::vector<UUID> GetDependents(UUID assetID) const;
		
		// ========== FILE WATCHING ==========
		
		// Check if any assets have been modified
		void UpdateFileWatchers();
		
		// Set callback for asset modifications
		void SetAssetModifiedCallback(std::function<void(UUID, const std::filesystem::path&)> callback) {
			m_OnAssetModified = callback;
		}
		
		// ========== UTILITIES ==========
		
		// Get asset type from file extension
		static AssetType GetAssetTypeFromExtension(const std::string& extension);
		
		// Get file extension for asset type
		static std::string GetExtensionForAssetType(AssetType type);
		
		// Get relative path from absolute path
		std::filesystem::path GetRelativePath(const std::filesystem::path& absolutePath) const;
		
		// Get absolute path from relative path
		std::filesystem::path GetAbsolutePath(const std::filesystem::path& relativePath) const;
		
		// ========== STATISTICS ==========
		
		size_t GetAssetCount() const { return m_Assets.size(); }
		size_t GetAssetCountByType(AssetType type) const;
		
	private:
		// Scan a directory recursively
		void ScanDirectory(const std::filesystem::path& directory);
		
		// Extract asset metadata from file
		AssetDatabaseEntry ExtractAssetMetadata(const std::filesystem::path& filePath);
		
		// Extract dependencies from asset file
		std::vector<UUID> ExtractDependencies(const std::filesystem::path& filePath, AssetType type);
		
		// Generate unique asset ID from file
		UUID GenerateAssetID(const std::filesystem::path& filePath);
		
	private:
		std::filesystem::path m_ProjectRoot;
		std::filesystem::path m_AssetsFolder;
		std::filesystem::path m_DatabasePath;
		
		// Asset registry (UUID -> Entry)
		std::unordered_map<UUID, AssetDatabaseEntry> m_Assets;
		
		// Path to UUID mapping for fast lookup
		std::unordered_map<std::string, UUID> m_PathToID;
		
		// Callback for asset modifications
		std::function<void(UUID, const std::filesystem::path&)> m_OnAssetModified;
		
		mutable std::mutex m_Mutex;
		bool m_IsInitialized = false;
	};

}
