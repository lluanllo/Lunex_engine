#pragma once

/**
 * @file ScriptSystem.h
 * @brief AAA Architecture: Main script system orchestrator
 *
 * ScriptSystem is an ISceneSystem that manages:
 * - Script lifecycle (create, update, destroy)
 * - Script compilation and hot-reload
 * - Batch processing for performance
 * - Physics event dispatching
 */

#include "Scene/Core/ISceneSystem.h"
#include "ScriptComponents.h"
#include "ScriptInstance.h"
#include "ScriptingEngine.h"
#include "entt.hpp"
#include <chrono>

namespace Lunex {

    // Forward declarations
    class Scene;
    struct SceneContext;

    /**
     * @class ScriptSystemAdvanced
     * @brief Advanced ISceneSystem implementation for script management
     *
     * NOTE: This is a secondary implementation. The primary ScriptSystem
     * used by Scene is in Scene/Systems/ScriptSystem.h
     */
    class ScriptSystemAdvanced : public ISceneSystem {
    public:
        ScriptSystemAdvanced();
        ~ScriptSystemAdvanced() override;

        // ========== ISceneSystem INTERFACE ==========

        void OnAttach(SceneContext& context) override;
        void OnDetach() override;

        void OnRuntimeStart(SceneMode mode) override;
        void OnRuntimeStop() override;

        void OnFixedUpdate(float fixedDeltaTime) override;
        void OnUpdate(Timestep ts, SceneMode mode) override;
        void OnLateUpdate(Timestep ts) override;

        void OnSceneEvent(const SceneSystemEvent& event) override;

        const std::string& GetName() const override { return m_Name; }
        SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Script; }

        bool IsActiveInMode(SceneMode mode) const override {
            // Scripts only run in Play mode
            return mode == SceneMode::Play;
        }

        // ========== SCRIPT MANAGEMENT API ==========

        /**
         * @brief Create a script instance for an entity
         * @param entity Entity to attach script to
         * @param scriptPath Path to .cpp script file
         * @return Instance ID (0 if failed)
         */
        uint32_t CreateScriptInstance(entt::entity entity, const std::string& scriptPath);

        /**
         * @brief Destroy a script instance
         */
        void DestroyScriptInstance(uint32_t instanceId);

        /**
         * @brief Get a script instance by ID
         */
        ScriptInstance* GetInstance(uint32_t instanceId);
        const ScriptInstance* GetInstance(uint32_t instanceId) const;

        /**
         * @brief Compile a script
         * @param scriptPath Source path
         * @param outDllPath Output DLL path
         * @return True if compilation succeeded
         */
        bool CompileScript(const std::string& scriptPath, std::string& outDllPath);

        /**
         * @brief Trigger hot-reload for all scripts
         */
        void HotReloadAll();

        /**
         * @brief Trigger hot-reload for a specific script
         */
        void HotReload(uint32_t instanceId);

        // ========== PHYSICS EVENT DISPATCHING ==========

        void DispatchCollisionEnter(entt::entity a, entt::entity b);
        void DispatchCollisionStay(entt::entity a, entt::entity b);
        void DispatchCollisionExit(entt::entity a, entt::entity b);

        void DispatchTriggerEnter(entt::entity a, entt::entity b);
        void DispatchTriggerStay(entt::entity a, entt::entity b);
        void DispatchTriggerExit(entt::entity a, entt::entity b);

        // ========== PROFILING ==========

        /**
         * @brief Get profiling data for a script instance
         */
        const ScriptProfilingData* GetProfilingData(uint32_t instanceId) const;

        /**
         * @brief Get total script execution time this frame
         */
        double GetFrameScriptTime() const { return m_FrameScriptTime; }

    private:
        // ========== INTERNAL HELPERS ==========

        void InitializeScript(entt::entity entity, ScriptMetadata& meta);
        void DestroyScript(entt::entity entity, ScriptMetadata& meta);
        void UpdateScript(ScriptInstance& instance, ScriptMetadata& meta, float deltaTime);

        void PrepareScriptContext(ScriptInstance& instance, float deltaTime);

        void SerializeAllScriptStates();
        void DeserializeAllScriptStates();

        // EnTT signal handlers
        void OnScriptComponentAdded(entt::registry& reg, entt::entity entity);
        void OnScriptComponentRemoved(entt::registry& reg, entt::entity entity);

    private:
        std::string m_Name = "ScriptSystemAdvanced";

        // Scene context (non-owning)
        SceneContext* m_Context = nullptr;

        // Script instance pool
        ScriptInstancePool m_InstancePool;

        // Scripting engine for compilation
        std::unique_ptr<ScriptingEngine> m_ScriptingEngine;

        // Frame metrics
        double m_FrameScriptTime = 0.0;
        uint64_t m_FrameCount = 0;
        float m_TotalTime = 0.0f;

        // Connection handles for EnTT signals
        entt::connection m_OnScriptAddedConnection;
        entt::connection m_OnScriptRemovedConnection;
    };

} // namespace Lunex
