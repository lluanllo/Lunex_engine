#pragma once

/**
 * @file ScriptComponents.h
 * @brief AAA Architecture: Data-oriented script components
 * 
 * ScriptMetadata is a POD component that stores minimal metadata.
 * Actual script instances are stored separately in ScriptSystem.
 */

#include <cstdint>
#include "entt.hpp"

namespace Lunex {

    /**
     * @enum ScriptFlags
     * @brief Bitfield flags for script state
     */
    enum class ScriptFlags : uint8_t {
        None            = 0,
        Enabled         = 1 << 0,    // Script is active
        Initialized     = 1 << 1,    // OnCreate has been called
        ErrorState      = 1 << 2,    // Script encountered an error
        MarkedForReload = 1 << 3,    // Pending hot-reload
        Paused          = 1 << 4     // Temporarily paused
    };

    inline ScriptFlags operator|(ScriptFlags a, ScriptFlags b) {
        return static_cast<ScriptFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    inline ScriptFlags operator&(ScriptFlags a, ScriptFlags b) {
        return static_cast<ScriptFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    inline ScriptFlags& operator|=(ScriptFlags& a, ScriptFlags b) {
        return a = a | b;
    }

    inline ScriptFlags& operator&=(ScriptFlags& a, ScriptFlags b) {
        return a = a & b;
    }

    inline ScriptFlags operator~(ScriptFlags a) {
        return static_cast<ScriptFlags>(~static_cast<uint8_t>(a));
    }

    inline bool HasFlag(ScriptFlags flags, ScriptFlags flag) {
        return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
    }

    /**
     * @struct ScriptMetadata
     * @brief POD component for script metadata (cache-friendly)
     * 
     * This is the minimal data stored per-entity in the ECS.
     * The actual script instance is stored in ScriptSystem's instance map.
     */
    struct ScriptMetadata {
        uint32_t instanceId = 0;                 // 4 bytes - ID in ScriptSystem's instance map
        ScriptFlags flags = ScriptFlags::Enabled; // 1 byte
        uint8_t scriptIndex = 0;                 // 1 byte - Index in entity's script list
        uint8_t _padding[2] = {0};               // 2 bytes - alignment padding
        
        // ========== INLINE HELPERS ==========
        
        bool IsEnabled() const {
            return HasFlag(flags, ScriptFlags::Enabled);
        }
        
        bool IsInitialized() const {
            return HasFlag(flags, ScriptFlags::Initialized);
        }
        
        bool HasError() const {
            return HasFlag(flags, ScriptFlags::ErrorState);
        }
        
        bool IsPaused() const {
            return HasFlag(flags, ScriptFlags::Paused);
        }
        
        bool NeedsReload() const {
            return HasFlag(flags, ScriptFlags::MarkedForReload);
        }
        
        void SetEnabled(bool enabled) {
            if (enabled) {
                flags |= ScriptFlags::Enabled;
            } else {
                flags &= ~ScriptFlags::Enabled;
            }
        }
        
        void SetError(bool error) {
            if (error) {
                flags |= ScriptFlags::ErrorState;
            } else {
                flags &= ~ScriptFlags::ErrorState;
            }
        }
        
        void MarkInitialized() {
            flags |= ScriptFlags::Initialized;
        }
        
        void MarkForReload() {
            flags |= ScriptFlags::MarkedForReload;
        }
        
        void ClearReloadFlag() {
            flags &= ~ScriptFlags::MarkedForReload;
        }
    };

    // Verify POD and alignment (8 bytes)
    static_assert(std::is_trivially_copyable_v<ScriptMetadata>, "ScriptMetadata must be trivially copyable");
    static_assert(sizeof(ScriptMetadata) == 8, "ScriptMetadata should be 8 bytes for cache efficiency");

    /**
     * @struct ScriptProfilingData
     * @brief Performance metrics for script execution
     */
    struct ScriptProfilingData {
        uint64_t updateCallCount = 0;
        double totalUpdateTime = 0.0;        // milliseconds
        double avgUpdateTime = 0.0;          // milliseconds
        double maxUpdateTime = 0.0;          // milliseconds
        double lastUpdateTime = 0.0;         // milliseconds
        
        void RecordUpdate(double elapsedMs) {
            updateCallCount++;
            totalUpdateTime += elapsedMs;
            avgUpdateTime = totalUpdateTime / static_cast<double>(updateCallCount);
            if (elapsedMs > maxUpdateTime) {
                maxUpdateTime = elapsedMs;
            }
            lastUpdateTime = elapsedMs;
        }
        
        void Reset() {
            updateCallCount = 0;
            totalUpdateTime = 0.0;
            avgUpdateTime = 0.0;
            maxUpdateTime = 0.0;
            lastUpdateTime = 0.0;
        }
    };

} // namespace Lunex
