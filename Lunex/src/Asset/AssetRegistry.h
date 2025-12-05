#pragma once

#include "Asset.h"

#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace Lunex {

	// ============================================================================
	// ASSET HANDLE
	// Lightweight reference to an asset that can be serialized
	// ============================================================================
	template<typename T>
	struct AssetHandle {
		UUID ID;
		
		AssetHandle() : ID(0) {}
		AssetHandle(UUID id) : ID(id) {}
		AssetHandle(const Ref<T>& asset) : ID(asset ? asset->GetID() : UUID(0)) {}
		
		bool IsValid() const { return ID != 0; }
		operator bool() const { return IsValid(); }
		operator UUID() const { return ID; }
		
		bool operator==(const AssetHandle& other) const { return ID == other.ID; }
		bool operator!=(const AssetHandle& other) const { return ID != other.ID; }
	};

	// ============================================================================
	// ASSET REGISTRY
	// Centralized management of all assets in the project
	// Features:
	// - Type-safe asset cache
	// - Path-to-UUID mapping
	// - Hot-reload support
	// - Factory pattern for asset creation
	// ============================================================================
	class AssetRegistry {
	public:
		// Singleton access
		static AssetRegistry& Get();
		
		// ========== INITIALIZATION ==========
		
		void Initialize();
		void Shutdown();
		void SetRootDirectory(const std::filesystem::path& path);
		const std::filesystem::path& GetRootDirectory() const { return m_RootDirectory; }
		
		// ========== ASSET LOADING ==========
		
		// Load asset from file (type-safe)
		template<typename T>
		Ref<T> Load(const std::filesystem::path& path);
		
		// Load asset by UUID
		template<typename T>
		Ref<T> Get(UUID id);
		
		// Get asset by path
		template<typename T>
		Ref<T> GetByPath(const std::filesystem::path& path);
		
		// Check if asset is loaded
		bool IsLoaded(UUID id) const;
		bool IsLoadedByPath(const std::filesystem::path& path) const;
		
		// ========== ASSET REGISTRATION ==========
		
		// Register a new or existing asset
		template<typename T>
		void Register(Ref<T> asset);
		
		// Unregister asset
		void Unregister(UUID id);
		void UnregisterByPath(const std::filesystem::path& path);
		
		// ========== ASSET CREATION ==========
		
		// Create new asset (not registered until saved)
		template<typename T, typename... Args>
		Ref<T> Create(const std::string& name, Args&&... args);
		
		// ========== ASSET QUERIES ==========
		
		// Get all assets of a specific type
		template<typename T>
		std::vector<Ref<T>> GetAllOfType();
		
		// Get all asset metadata
		std::vector<AssetMetadata> GetAllMetadata() const;
		std::vector<AssetMetadata> GetMetadataByType(AssetType type) const;
		
		// Search assets by name
		template<typename T>
		std::vector<Ref<T>> SearchByName(const std::string& query);
		
		// ========== HOT-RELOAD ==========
		
		// Reload a specific asset
		void Reload(UUID id);
		void ReloadByPath(const std::filesystem::path& path);
		
		// Check for modified files and reload
		void ReloadModified();
		
		// Update (call periodically for hot-reload)
		void Update(float deltaTime);
		
		// ========== CLEANUP ==========
		
		// Remove assets with no references
		void ClearUnused();
		
		// Clear all assets
		void ClearAll();
		
		// ========== STATISTICS ==========
		
		size_t GetAssetCount() const;
		size_t GetAssetCountByType(AssetType type) const;
		
	private:
		AssetRegistry() = default;
		~AssetRegistry() = default;
		
		// Non-copyable
		AssetRegistry(const AssetRegistry&) = delete;
		AssetRegistry& operator=(const AssetRegistry&) = delete;
		
		// Internal helpers
		std::string NormalizePath(const std::filesystem::path& path) const;
		void AddFileWatcher(const std::filesystem::path& path, UUID assetID);
		void RemoveFileWatcher(const std::filesystem::path& path);
		void UpdateFileTimestamps();
		
		// Get type-specific cache
		template<typename T>
		std::unordered_map<UUID, Ref<T>>& GetCache();
		
	private:
		std::filesystem::path m_RootDirectory;
		
		// Asset caches by type (use unordered_map for O(1) lookup)
		std::unordered_map<UUID, Ref<Asset>> m_AssetCache;
		
		// Path to UUID mapping for fast lookup
		std::unordered_map<std::string, UUID> m_PathToUUID;
		
		// Asset metadata for all known assets (including unloaded)
		std::unordered_map<UUID, AssetMetadata> m_Metadata;
		
		// File watching for hot-reload
		struct FileWatchData {
			std::filesystem::path Path;
			std::filesystem::file_time_type LastModified;
			UUID AssetID;
		};
		std::unordered_map<std::string, FileWatchData> m_FileWatchers;
		
		// Hot-reload timing
		float m_TimeSinceLastCheck = 0.0f;
		static constexpr float s_FileCheckInterval = 1.0f; // Check every second
		
		// Thread safety
		mutable std::mutex m_Mutex;
		
		bool m_Initialized = false;
	};

	// ============================================================================
	// TEMPLATE IMPLEMENTATIONS
	// ============================================================================

	template<typename T>
	Ref<T> AssetRegistry::Load(const std::filesystem::path& path) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		// Normalize path for consistent lookup
		std::string normalizedPath = NormalizePath(path);
		
		// Check if already loaded
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			auto cacheIt = m_AssetCache.find(it->second);
			if (cacheIt != m_AssetCache.end()) {
				return std::dynamic_pointer_cast<T>(cacheIt->second);
			}
		}
		
		// Load from file (implemented by derived class)
		Ref<T> asset = T::LoadFromFile(path);
		if (!asset) {
			return nullptr;
		}
		
		// Register in cache
		UUID id = asset->GetID();
		m_AssetCache[id] = asset;
		m_PathToUUID[normalizedPath] = id;
		m_Metadata[id] = asset->GetMetadata();
		
		// Setup file watcher
		AddFileWatcher(path, id);
		
		return asset;
	}

	template<typename T>
	Ref<T> AssetRegistry::Get(UUID id) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_AssetCache.find(id);
		if (it != m_AssetCache.end()) {
			return std::dynamic_pointer_cast<T>(it->second);
		}
		return nullptr;
	}

	template<typename T>
	Ref<T> AssetRegistry::GetByPath(const std::filesystem::path& path) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::string normalizedPath = NormalizePath(path);
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			return Get<T>(it->second);
		}
		return nullptr;
	}

	template<typename T>
	void AssetRegistry::Register(Ref<T> asset) {
		if (!asset) return;
		
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		UUID id = asset->GetID();
		m_AssetCache[id] = asset;
		m_Metadata[id] = asset->GetMetadata();
		
		if (!asset->GetPath().empty()) {
			std::string normalizedPath = NormalizePath(asset->GetPath());
			m_PathToUUID[normalizedPath] = id;
			AddFileWatcher(asset->GetPath(), id);
		}
	}

	template<typename T, typename... Args>
	Ref<T> AssetRegistry::Create(const std::string& name, Args&&... args) {
		Ref<T> asset = CreateRef<T>(name, std::forward<Args>(args)...);
		// Note: Not registered until saved with a path
		return asset;
	}

	template<typename T>
	std::vector<Ref<T>> AssetRegistry::GetAllOfType() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<Ref<T>> assets;
		for (const auto& [id, asset] : m_AssetCache) {
			if (Ref<T> typedAsset = std::dynamic_pointer_cast<T>(asset)) {
				assets.push_back(typedAsset);
			}
		}
		return assets;
	}

	template<typename T>
	std::vector<Ref<T>> AssetRegistry::SearchByName(const std::string& query) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<Ref<T>> results;
		std::string lowerQuery = query;
		std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
		
		for (const auto& [id, asset] : m_AssetCache) {
			if (Ref<T> typedAsset = std::dynamic_pointer_cast<T>(asset)) {
				std::string name = typedAsset->GetName();
				std::transform(name.begin(), name.end(), name.begin(), ::tolower);
				
				if (name.find(lowerQuery) != std::string::npos) {
					results.push_back(typedAsset);
				}
			}
		}
		return results;
	}

}
