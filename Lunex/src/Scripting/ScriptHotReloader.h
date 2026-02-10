#pragma once

/**
 * @file ScriptHotReloader.h
 * @brief AAA Architecture: Hot-reload system for scripts
 *
 * Monitors script files for changes and reloads them automatically
 * while preserving script state.
 */

#include "ScriptSystem.h"
#include "ScriptCompiler.h"
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

namespace Lunex {

    /**
     * @struct HotReloadConfig
     * @brief Configuration for hot-reload behavior
     */
    struct HotReloadConfig {
        bool enabled = true;
        bool autoDetect = true;                    // Auto-detect file changes
        float checkIntervalSeconds = 1.0f;         // How often to check for changes
        bool preserveState = true;                 // Preserve script state on reload
        bool recompileOnChange = true;             // Auto-recompile changed scripts
        uint32_t debounceMs = 100;                 // Debounce rapid changes
    };

    /**
     * @struct FileWatchEntry
     * @brief Tracks a single file for changes
     */
    struct FileWatchEntry {
        std::string path;
        std::filesystem::file_time_type lastModified;
        uint32_t instanceId;
        bool pendingReload = false;
    };

    /**
     * @class ScriptHotReloader
     * @brief Manages hot-reloading of scripts
     */
    class ScriptHotReloader {
    public:
        ScriptHotReloader(ScriptSystemAdvanced& scriptSystem);
        ~ScriptHotReloader();

        /**
         * @brief Set hot-reload configuration
         */
        void SetConfig(const HotReloadConfig& config) { m_Config = config; }
        const HotReloadConfig& GetConfig() const { return m_Config; }

        /**
         * @brief Enable or disable hot-reload
         */
        void SetEnabled(bool enabled) { m_Config.enabled = enabled; }
        bool IsEnabled() const { return m_Config.enabled; }

        /**
         * @brief Start watching for file changes
         */
        void StartWatching();

        /**
         * @brief Stop watching for file changes
         */
        void StopWatching();

        /**
         * @brief Update (call every frame to check for changes)
         */
        void Update();

        /**
         * @brief Force reload of a specific script
         */
        void ForceReload(uint32_t instanceId);

        /**
         * @brief Force reload of all scripts
         */
        void ForceReloadAll();

        /**
         * @brief Register a script for watching
         */
        void WatchScript(uint32_t instanceId, const std::string& sourcePath);

        /**
         * @brief Unregister a script from watching
         */
        void UnwatchScript(uint32_t instanceId);

        /**
         * @brief Get list of scripts pending reload
         */
        std::vector<uint32_t> GetPendingReloads() const;

        /**
         * @brief Get reload statistics
         */
        struct ReloadStats {
            uint32_t totalReloads = 0;
            uint32_t successfulReloads = 0;
            uint32_t failedReloads = 0;
            double lastReloadTimeMs = 0.0;
        };
        const ReloadStats& GetStats() const { return m_Stats; }

    private:
        void CheckForChanges();
        void ProcessPendingReloads();
        bool ReloadScript(uint32_t instanceId);

        std::filesystem::file_time_type GetFileModTime(const std::string& path);

    private:
        ScriptSystemAdvanced& m_ScriptSystem;
        ScriptCompiler m_Compiler;
        HotReloadConfig m_Config;
        ReloadStats m_Stats;

        // File watching
        std::unordered_map<uint32_t, FileWatchEntry> m_WatchedFiles;
        std::vector<uint32_t> m_PendingReloads;

        // Timing
        std::chrono::steady_clock::time_point m_LastCheckTime;

        // Thread safety
        mutable std::mutex m_Mutex;
        std::atomic<bool> m_IsWatching{ false };
    };

} // namespace Lunex
