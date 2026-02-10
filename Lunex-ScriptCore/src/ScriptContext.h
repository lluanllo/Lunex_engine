#pragma once

/**
 * @file ScriptContext.h
 * @brief AAA Architecture: Rich context provided to all scripts
 * 
 * ScriptContext provides scripts with access to:
 * - Entity component access via EngineContext callbacks
 * - Entity lifecycle (Create, Destroy)
 * - Frame data (deltaTime, time, frameCount)
 * - Logging
 * 
 * Note: This context is designed for DLL boundary safety.
 * It uses opaque pointers and callbacks instead of direct EnTT access.
 */

#include "LunexScriptingAPI.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Lunex {

    /**
     * @class ScriptContext
     * @brief Context object injected into scripts providing engine access
     * 
     * This is the main interface scripts use to interact with the engine.
     * All pointers are non-owning references to engine systems.
     */
    class ScriptContext {
    public:
        // ========== CORE REFERENCES ==========
        EngineContext* engineContext = nullptr;
        void* entityHandle = nullptr;      // Opaque entity handle
        
        // ========== FRAME DATA ==========
        float deltaTime = 0.0f;
        float fixedDeltaTime = 0.02f;
        float totalTime = 0.0f;
        uint64_t frameCount = 0;
        
        // ========== CONVENIENCE GETTERS ==========
        
        void* GetEntityHandle() const { return entityHandle; }
        float DeltaTime() const { return deltaTime; }
        float Time() const { return totalTime; }
        uint64_t FrameCount() const { return frameCount; }
        
        // ========== ENTITY LIFECYCLE API ==========
        
        void* CreateEntity(const std::string& name = "Entity") {
            if (!engineContext || !engineContext->CreateEntity) {
                return nullptr;
            }
            return engineContext->CreateEntity(name.c_str());
        }
        
        void DestroyEntity(void* entity = nullptr) {
            if (!engineContext || !engineContext->DestroyEntity) {
                return;
            }
            void* target = entity ? entity : entityHandle;
            if (target) {
                engineContext->DestroyEntity(target);
            }
        }
        
        // ========== LOGGING API ==========
        
        void LogInfo(const std::string& message) {
            if (engineContext && engineContext->LogInfo) {
                engineContext->LogInfo(message.c_str());
            }
        }
        
        void LogWarning(const std::string& message) {
            if (engineContext && engineContext->LogWarning) {
                engineContext->LogWarning(message.c_str());
            }
        }
        
        void LogError(const std::string& message) {
            if (engineContext && engineContext->LogError) {
                engineContext->LogError(message.c_str());
            }
        }
    };

} // namespace Lunex
