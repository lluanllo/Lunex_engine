#pragma once

/**
 * @file AssetRegistry.h
 * @brief Centralized asset management and caching
 * 
 * Part of the Unified Assets System
 */

#include "Asset.h"
#include "AssetHandle.h"

#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace Lunex {

	// ============================================================================
	// ASSET REGISTRY - Centralized asset management
	// ============================================================================
	class AssetRegistry {
	public:
		static AssetRegistry& Get();
		
		// Initialization
		void Initialize();
		void Shutdown();
		void SetRootDirectory(const std::filesystem::path& path);
		const std::filesystem::path& GetRootDirectory() const { return m_RootDirectory; }
		
		// Asset loading
		template<typename T>
		Ref<T> Load(const std::filesystem::path& path);
		
		template<typename T>
		Ref<T> Get(UUID id);
		
		template<typename T>
		Ref<T> GetByPath(const std::filesystem::path& path);
		
		bool IsLoaded(UUID id) const;
		bool IsLoadedByPath(const std::filesystem::path& path) const;
		
		// Registration
		template<typename T>
		void Register(Ref<T> asset);
		
		void Unregister(UUID id);
		void UnregisterByPath(const std::filesystem::path& path);
		
		// Creation
		template<typename T, typename... Args>
		Ref<T> Create(const std::string& name, Args&&... args);
		
		// Queries
		template<typename T>
		std::vector<Ref<T>> GetAllOfType();
		
		std::vector<AssetMetadata> GetAllMetadata() const;
		std::vector<AssetMetadata> GetMetadataByType(AssetType type) const;
		
		template<typename T>
		std::vector<Ref<T>> SearchByName(const std::string& query);
		
		// Hot-reload
		void Reload(UUID id);
		void ReloadByPath(const std::filesystem::path& path);
		void ReloadModified();
		void Update(float deltaTime);
		
		// Cleanup
		void ClearUnused();
		void ClearAll();
		
		// Statistics
		size_t GetAssetCount() const;
		size_t GetAssetCountByType(AssetType type) const;
		
	private:
		AssetRegistry() = default;
		~AssetRegistry() = default;
		
		AssetRegistry(const AssetRegistry&) = delete;
		AssetRegistry& operator=(const AssetRegistry&) = delete;
		
		std::string NormalizePath(const std::filesystem::path& path) const;
		void AddFileWatcher(const std::filesystem::path& path, UUID assetID);
		void RemoveFileWatcher(const std::filesystem::path& path);
		void UpdateFileTimestamps();
		
	private:
		std::filesystem::path m_RootDirectory;
		std::unordered_map<UUID, Ref<Asset>> m_AssetCache;
		std::unordered_map<std::string, UUID> m_PathToUUID;
		std::unordered_map<UUID, AssetMetadata> m_Metadata;
		
		struct FileWatchData {
			std::filesystem::path Path;
			std::filesystem::file_time_type LastModified;
			UUID AssetID;
		};
		std::unordered_map<std::string, FileWatchData> m_FileWatchers;
		
		float m_TimeSinceLastCheck = 0.0f;
		static constexpr float s_FileCheckInterval = 1.0f;
		
		mutable std::mutex m_Mutex;
		bool m_Initialized = false;
	};

	// ============================================================================
	// TEMPLATE IMPLEMENTATIONS
	// ============================================================================

	template<typename T>
	Ref<T> AssetRegistry::Load(const std::filesystem::path& path) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::string normalizedPath = NormalizePath(path);
		
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			auto cacheIt = m_AssetCache.find(it->second);
			if (cacheIt != m_AssetCache.end()) {
				return std::dynamic_pointer_cast<T>(cacheIt->second);
			}
		}
		
		Ref<T> asset = T::LoadFromFile(path);
		if (!asset) {
			return nullptr;
		}
		
		UUID id = asset->GetID();
		m_AssetCache[id] = asset;
		m_PathToUUID[normalizedPath] = id;
		m_Metadata[id] = asset->GetMetadata();
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
		std::string normalizedPath = NormalizePath(path);
		
		std::lock_guard<std::mutex> lock(m_Mutex);
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
		return CreateRef<T>(name, std::forward<Args>(args)...);
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
