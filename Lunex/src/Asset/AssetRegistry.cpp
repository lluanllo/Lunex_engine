#include "stpch.h"
#include "AssetRegistry.h"

#include "Log/Log.h"

#include <algorithm>

namespace Lunex {

	AssetRegistry& AssetRegistry::Get() {
		static AssetRegistry instance;
		return instance;
	}

	void AssetRegistry::Initialize() {
		if (m_Initialized) return;
		
		LNX_LOG_INFO("AssetRegistry initialized");
		m_Initialized = true;
	}

	void AssetRegistry::Shutdown() {
		if (!m_Initialized) return;
		
		ClearAll();
		m_Initialized = false;
		LNX_LOG_INFO("AssetRegistry shutdown");
	}

	void AssetRegistry::SetRootDirectory(const std::filesystem::path& path) {
		m_RootDirectory = path;
		LNX_LOG_INFO("AssetRegistry root directory set to: {0}", path.string());
	}

	bool AssetRegistry::IsLoaded(UUID id) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_AssetCache.find(id) != m_AssetCache.end();
	}

	bool AssetRegistry::IsLoadedByPath(const std::filesystem::path& path) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		std::string normalizedPath = NormalizePath(path);
		return m_PathToUUID.find(normalizedPath) != m_PathToUUID.end();
	}

	void AssetRegistry::Unregister(UUID id) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_AssetCache.find(id);
		if (it != m_AssetCache.end()) {
			// Remove from path mapping
			if (!it->second->GetPath().empty()) {
				std::string normalizedPath = NormalizePath(it->second->GetPath());
				m_PathToUUID.erase(normalizedPath);
				RemoveFileWatcher(it->second->GetPath());
			}
			
			// Remove from metadata
			m_Metadata.erase(id);
			
			// Remove from cache
			m_AssetCache.erase(it);
			
			LNX_LOG_INFO("Asset unregistered: {0}", (uint64_t)id);
		}
	}

	void AssetRegistry::UnregisterByPath(const std::filesystem::path& path) {
		std::string normalizedPath = NormalizePath(path);
		
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			UUID id = it->second;
			m_Mutex.unlock();  // Avoid deadlock
			Unregister(id);
		}
	}

	std::vector<AssetMetadata> AssetRegistry::GetAllMetadata() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<AssetMetadata> result;
		result.reserve(m_Metadata.size());
		
		for (const auto& [id, metadata] : m_Metadata) {
			result.push_back(metadata);
		}
		return result;
	}

	std::vector<AssetMetadata> AssetRegistry::GetMetadataByType(AssetType type) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<AssetMetadata> result;
		for (const auto& [id, metadata] : m_Metadata) {
			if (metadata.Type == type) {
				result.push_back(metadata);
			}
		}
		return result;
	}

	void AssetRegistry::Reload(UUID id) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_AssetCache.find(id);
		if (it == m_AssetCache.end()) {
			LNX_LOG_WARN("AssetRegistry::Reload - Asset not found: {0}", (uint64_t)id);
			return;
		}
		
		std::filesystem::path path = it->second->GetPath();
		if (path.empty() || !std::filesystem::exists(path)) {
			LNX_LOG_WARN("AssetRegistry::Reload - Invalid path for asset: {0}", (uint64_t)id);
			return;
		}
		
		// Store asset type for reload
		AssetType type = it->second->GetType();
		
		// Remove old asset
		m_AssetCache.erase(it);
		
		// Note: Actual reload is type-specific and should be handled by the derived class
		// This is a placeholder - in practice, we'd call the appropriate LoadFromFile
		
		LNX_LOG_INFO("Asset reloaded: {0}", path.string());
	}

	void AssetRegistry::ReloadByPath(const std::filesystem::path& path) {
		std::string normalizedPath = NormalizePath(path);
		
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			UUID id = it->second;
			m_Mutex.unlock();
			Reload(id);
		}
	}

	void AssetRegistry::ReloadModified() {
		UpdateFileTimestamps();
	}

	void AssetRegistry::Update(float deltaTime) {
		m_TimeSinceLastCheck += deltaTime;
		
		if (m_TimeSinceLastCheck >= s_FileCheckInterval) {
			ReloadModified();
			m_TimeSinceLastCheck = 0.0f;
		}
	}

	void AssetRegistry::ClearUnused() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<UUID> toRemove;
		
		for (const auto& [id, asset] : m_AssetCache) {
			// Check if only the registry holds a reference
			if (asset.use_count() == 1) {
				toRemove.push_back(id);
			}
		}
		
		for (UUID id : toRemove) {
			auto it = m_AssetCache.find(id);
			if (it != m_AssetCache.end()) {
				if (!it->second->GetPath().empty()) {
					std::string normalizedPath = NormalizePath(it->second->GetPath());
					m_PathToUUID.erase(normalizedPath);
					RemoveFileWatcher(it->second->GetPath());
				}
				m_AssetCache.erase(it);
			}
		}
		
		if (!toRemove.empty()) {
			LNX_LOG_INFO("Cleared {0} unused assets", toRemove.size());
		}
	}

	void AssetRegistry::ClearAll() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		m_AssetCache.clear();
		m_PathToUUID.clear();
		m_Metadata.clear();
		m_FileWatchers.clear();
		
		LNX_LOG_INFO("AssetRegistry cleared");
	}

	size_t AssetRegistry::GetAssetCount() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_AssetCache.size();
	}

	size_t AssetRegistry::GetAssetCountByType(AssetType type) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		size_t count = 0;
		for (const auto& [id, asset] : m_AssetCache) {
			if (asset->GetType() == type) {
				count++;
			}
		}
		return count;
	}

	std::string AssetRegistry::NormalizePath(const std::filesystem::path& path) const {
		try {
			std::filesystem::path normalized;
			
			if (path.is_absolute()) {
				normalized = std::filesystem::weakly_canonical(path);
			} else if (!m_RootDirectory.empty()) {
				normalized = std::filesystem::weakly_canonical(m_RootDirectory / path);
			} else {
				normalized = std::filesystem::weakly_canonical(path);
			}
			
			std::string pathStr = normalized.string();
			std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
			
			#ifdef _WIN32
			std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
			#endif
			
			return pathStr;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to normalize path: {0}", e.what());
			return path.string();
		}
	}

	void AssetRegistry::AddFileWatcher(const std::filesystem::path& path, UUID assetID) {
		if (!std::filesystem::exists(path)) return;
		
		std::string normalizedPath = NormalizePath(path);
		
		FileWatchData data;
		data.Path = path;
		data.LastModified = std::filesystem::last_write_time(path);
		data.AssetID = assetID;
		
		m_FileWatchers[normalizedPath] = data;
	}

	void AssetRegistry::RemoveFileWatcher(const std::filesystem::path& path) {
		std::string normalizedPath = NormalizePath(path);
		m_FileWatchers.erase(normalizedPath);
	}

	void AssetRegistry::UpdateFileTimestamps() {
		std::vector<UUID> assetsToReload;
		
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			
			for (auto& [pathStr, watchData] : m_FileWatchers) {
				if (!std::filesystem::exists(watchData.Path)) continue;
				
				try {
					auto currentModified = std::filesystem::last_write_time(watchData.Path);
					if (currentModified != watchData.LastModified) {
						watchData.LastModified = currentModified;
						assetsToReload.push_back(watchData.AssetID);
					}
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("Failed to check file modification time: {0}", e.what());
				}
			}
		}
		
		// Reload outside of lock to avoid deadlock
		for (UUID id : assetsToReload) {
			Reload(id);
		}
	}

}
