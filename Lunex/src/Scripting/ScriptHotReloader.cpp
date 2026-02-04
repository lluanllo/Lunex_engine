#include "stpch.h"
#include "ScriptHotReloader.h"

namespace Lunex {

    ScriptHotReloader::ScriptHotReloader(ScriptSystem& scriptSystem)
        : m_ScriptSystem(scriptSystem)
    {
        m_LastCheckTime = std::chrono::steady_clock::now();
    }

    ScriptHotReloader::~ScriptHotReloader() {
        StopWatching();
    }

    void ScriptHotReloader::StartWatching() {
        m_IsWatching = true;
        LNX_LOG_INFO("[ScriptHotReloader] Started watching for file changes");
    }

    void ScriptHotReloader::StopWatching() {
        m_IsWatching = false;
        LNX_LOG_INFO("[ScriptHotReloader] Stopped watching for file changes");
    }

    void ScriptHotReloader::Update() {
        if (!m_Config.enabled || !m_IsWatching) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastCheckTime);
        
        // Check at configured interval
        if (elapsed.count() >= static_cast<long long>(m_Config.checkIntervalSeconds * 1000.0f)) {
            CheckForChanges();
            m_LastCheckTime = now;
        }

        // Process pending reloads
        ProcessPendingReloads();
    }

    void ScriptHotReloader::ForceReload(uint32_t instanceId) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        auto it = m_WatchedFiles.find(instanceId);
        if (it != m_WatchedFiles.end()) {
            it->second.pendingReload = true;
            
            // Add to pending if not already there
            if (std::find(m_PendingReloads.begin(), m_PendingReloads.end(), instanceId) == m_PendingReloads.end()) {
                m_PendingReloads.push_back(instanceId);
            }
        }
    }

    void ScriptHotReloader::ForceReloadAll() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        for (auto& [id, entry] : m_WatchedFiles) {
            entry.pendingReload = true;
            
            if (std::find(m_PendingReloads.begin(), m_PendingReloads.end(), id) == m_PendingReloads.end()) {
                m_PendingReloads.push_back(id);
            }
        }
        
        LNX_LOG_INFO("[ScriptHotReloader] Queued {0} scripts for reload", m_PendingReloads.size());
    }

    void ScriptHotReloader::WatchScript(uint32_t instanceId, const std::string& sourcePath) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        FileWatchEntry entry;
        entry.path = sourcePath;
        entry.instanceId = instanceId;
        entry.lastModified = GetFileModTime(sourcePath);
        entry.pendingReload = false;
        
        m_WatchedFiles[instanceId] = entry;
        
        LNX_LOG_INFO("[ScriptHotReloader] Watching: {0}", sourcePath);
    }

    void ScriptHotReloader::UnwatchScript(uint32_t instanceId) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        auto it = m_WatchedFiles.find(instanceId);
        if (it != m_WatchedFiles.end()) {
            LNX_LOG_INFO("[ScriptHotReloader] Unwatching: {0}", it->second.path);
            m_WatchedFiles.erase(it);
        }
        
        // Remove from pending
        m_PendingReloads.erase(
            std::remove(m_PendingReloads.begin(), m_PendingReloads.end(), instanceId),
            m_PendingReloads.end()
        );
    }

    std::vector<uint32_t> ScriptHotReloader::GetPendingReloads() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_PendingReloads;
    }

    void ScriptHotReloader::CheckForChanges() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        for (auto& [id, entry] : m_WatchedFiles) {
            if (entry.pendingReload) continue; // Already pending
            
            auto currentModTime = GetFileModTime(entry.path);
            
            if (currentModTime != entry.lastModified) {
                LNX_LOG_INFO("[ScriptHotReloader] Change detected: {0}", entry.path);
                
                entry.lastModified = currentModTime;
                entry.pendingReload = true;
                
                // Add to pending
                if (std::find(m_PendingReloads.begin(), m_PendingReloads.end(), id) == m_PendingReloads.end()) {
                    m_PendingReloads.push_back(id);
                }
            }
        }
    }

    void ScriptHotReloader::ProcessPendingReloads() {
        std::vector<uint32_t> toReload;
        
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            toReload = std::move(m_PendingReloads);
            m_PendingReloads.clear();
        }
        
        if (toReload.empty()) return;
        
        LNX_LOG_INFO("[ScriptHotReloader] Processing {0} pending reloads...", toReload.size());
        
        for (uint32_t instanceId : toReload) {
            if (ReloadScript(instanceId)) {
                m_Stats.successfulReloads++;
            } else {
                m_Stats.failedReloads++;
            }
            m_Stats.totalReloads++;
            
            // Clear pending flag
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_WatchedFiles.find(instanceId);
            if (it != m_WatchedFiles.end()) {
                it->second.pendingReload = false;
            }
        }
    }

    bool ScriptHotReloader::ReloadScript(uint32_t instanceId) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        LNX_LOG_INFO("[ScriptHotReloader] Reloading script instance {0}...", instanceId);
        
        try {
            m_ScriptSystem.HotReload(instanceId);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            m_Stats.lastReloadTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            LNX_LOG_INFO("[ScriptHotReloader] Reload successful ({0:.2f}ms)", m_Stats.lastReloadTimeMs);
            return true;
        }
        catch (const std::exception& e) {
            LNX_LOG_ERROR("[ScriptHotReloader] Reload failed: {0}", e.what());
            return false;
        }
    }

    std::filesystem::file_time_type ScriptHotReloader::GetFileModTime(const std::string& path) {
        namespace fs = std::filesystem;
        
        if (!fs::exists(path)) {
            return fs::file_time_type::min();
        }
        
        try {
            return fs::last_write_time(path);
        }
        catch (const std::exception&) {
            return fs::file_time_type::min();
        }
    }

} // namespace Lunex
