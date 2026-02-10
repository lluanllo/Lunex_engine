#include "stpch.h"
#include "ScriptSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Scene/Core/SceneEvents.h"
#include "Scene/Components.h"
#include "Core/JobSystem/JobSystem.h"

namespace Lunex {

    ScriptSystemAdvanced::ScriptSystemAdvanced() {
        m_ScriptingEngine = std::make_unique<ScriptingEngine>();
    }

    ScriptSystemAdvanced::~ScriptSystemAdvanced() {
        // Don't call OnDetach in destructor - registry may already be destroyed
        m_InstancePool.Clear();
    }

    // ========================================================================
    // ISceneSystem Interface Implementation
    // ========================================================================

    void ScriptSystemAdvanced::OnAttach(SceneContext& context) {
        m_Context = &context;

        // Initialize the scripting engine with the scene
        if (context.ActiveScene) {
            m_ScriptingEngine->Initialize(context.ActiveScene);
        }

        // NOTE: Do NOT connect to ScriptMetadata signals here.
        // ScriptMetadata is not used as an ECS component in the current architecture.
        // Connecting signals for unused component types forces entt to create
        // internal storage that can corrupt the registry's dense_map.

        LNX_LOG_INFO("[ScriptSystemAdvanced] Attached to scene");
    }

    void ScriptSystemAdvanced::OnDetach() {
        if (!m_Context) return;

        // Clean up all script instances
        m_InstancePool.Clear();

        m_Context = nullptr;

        LNX_LOG_INFO("[ScriptSystemAdvanced] Detached from scene");
    }

    void ScriptSystemAdvanced::OnRuntimeStart(SceneMode mode) {
        if (mode != SceneMode::Play) return;
        if (!m_Context || !m_Context->Registry) return;

        LNX_LOG_INFO("[ScriptSystemAdvanced] Runtime starting...");

        // Initialize all scripts that have ScriptComponent
        auto view = m_Context->Registry->view<ScriptComponent, IDComponent>();

        for (auto entity : view) {
            auto& scriptComp = view.get<ScriptComponent>(entity);
            auto& idComp = view.get<IDComponent>(entity);

            // Process each script in the component
            for (size_t i = 0; i < scriptComp.GetScriptCount(); i++) {
                const std::string& scriptPath = scriptComp.GetScriptPath(i);
                if (scriptPath.empty()) continue;

                // Compile script if needed
                std::string dllPath;
                if (scriptComp.AutoCompile) {
                    if (!m_ScriptingEngine->CompileScript(scriptPath, dllPath)) {
                        LNX_LOG_ERROR("[ScriptSystemAdvanced] Failed to compile script: {0}", scriptPath);
                        continue;
                    }

                    // Store compiled path
                    if (i < scriptComp.CompiledDLLPaths.size()) {
                        scriptComp.CompiledDLLPaths[i] = dllPath;
                    }
                }

                // Create instance
                uint32_t instanceId = CreateScriptInstance(entity, scriptPath);
                if (instanceId == 0) {
                    LNX_LOG_ERROR("[ScriptSystemAdvanced] Failed to create script instance: {0}", scriptPath);
                    continue;
                }

                // Store instance pointer
                auto* instance = GetInstance(instanceId);
                if (instance && i < scriptComp.ScriptPluginInstances.size()) {
                    scriptComp.ScriptPluginInstances[i] = instance->plugin.get();
                    scriptComp.ScriptLoadedStates[i] = true;
                }
            }
        }

        // Call OnPlayModeEnter on all loaded scripts
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            if (instance.IsLoaded()) {
                instance.plugin->OnPlayModeEnter();
            }
            });

        LNX_LOG_INFO("[ScriptSystemAdvanced] Runtime started with {0} scripts", m_InstancePool.GetActiveCount());
    }

    void ScriptSystemAdvanced::OnRuntimeStop() {
        if (!m_Context || !m_Context->Registry) return;

        LNX_LOG_INFO("[ScriptSystemAdvanced] Runtime stopping...");

        // Call OnPlayModeExit on all scripts
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            if (instance.IsLoaded()) {
                instance.plugin->OnPlayModeExit();
            }
            });

        // Unload all scripts
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            if (instance.plugin) {
                instance.plugin->Unload();
            }
            });

        // Clear instance pool
        m_InstancePool.Clear();

        // Clear script component states
        auto view = m_Context->Registry->view<ScriptComponent>();
        for (auto entity : view) {
            auto& scriptComp = view.get<ScriptComponent>(entity);
            for (size_t i = 0; i < scriptComp.ScriptLoadedStates.size(); i++) {
                scriptComp.ScriptLoadedStates[i] = false;
                scriptComp.ScriptPluginInstances[i] = nullptr;
            }
        }

        LNX_LOG_INFO("[ScriptSystemAdvanced] Runtime stopped");
    }

    void ScriptSystemAdvanced::OnFixedUpdate(float fixedDeltaTime) {
        // Fixed update for physics-related script callbacks
        // Currently not used - would call OnFixedUpdate on scripts
    }

    void ScriptSystemAdvanced::OnUpdate(Timestep ts, SceneMode mode) {
        if (mode != SceneMode::Play) return;
        if (!m_Context || !m_Context->Registry) return;

        m_FrameCount++;
        m_TotalTime += ts;

        auto startTime = std::chrono::high_resolution_clock::now();

        // Collect all active scripts
        std::vector<std::pair<uint32_t, ScriptInstance*>> activeScripts;
        m_InstancePool.ForEach([&activeScripts](uint32_t id, ScriptInstance& instance) {
            if (instance.IsLoaded()) {
                activeScripts.push_back({ id, &instance });
            }
            });

        // Update scripts (could be parallelized with JobSystem)
        for (auto& [id, instance] : activeScripts) {
            auto scriptStart = std::chrono::high_resolution_clock::now();

            // Update context
            PrepareScriptContext(*instance, ts);

            // Call Update
            instance->plugin->Update(ts);

            // Record profiling
            auto scriptEnd = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(scriptEnd - scriptStart).count();
            instance->profiling.RecordUpdate(elapsed);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        m_FrameScriptTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    }

    void ScriptSystemAdvanced::OnLateUpdate(Timestep ts) {
        // Late update for scripts that need to run after other systems
        // Currently not used
    }

    void ScriptSystemAdvanced::OnSceneEvent(const SceneSystemEvent& event) {
        // Handle entity created/destroyed events
        // For now, rely on EnTT signals
    }

    // ========================================================================
    // Script Management API
    // ========================================================================

    uint32_t ScriptSystemAdvanced::CreateScriptInstance(entt::entity entity, const std::string& scriptPath) {
        if (!m_Context || !m_Context->Registry) return 0;

        // Get or compile DLL path
        std::string dllPath;
        if (!m_ScriptingEngine->CompileScript(scriptPath, dllPath)) {
            LNX_LOG_ERROR("[ScriptSystemAdvanced] Failed to compile script: {0}", scriptPath);
            return 0;
        }

        // Acquire instance from pool
        auto [instanceId, instance] = m_InstancePool.Acquire();

        // Create plugin
        instance->plugin = std::make_unique<ScriptPlugin>();
        instance->sourcePath = scriptPath;
        instance->dllPath = dllPath;
        instance->ownerEntity = entity;

        // Set current entity in context
        auto* ctx = m_ScriptingEngine->GetEngineContext();
        ctx->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));

        // Load the DLL
        if (!instance->plugin->Load(dllPath, ctx)) {
            LNX_LOG_ERROR("[ScriptSystemAdvanced] Failed to load script DLL: {0}", dllPath);
            m_InstancePool.Release(instanceId);
            return 0;
        }

        LNX_LOG_INFO("[ScriptSystemAdvanced] Created script instance {0} for entity {1}",
            instanceId, static_cast<uint32_t>(entity));

        return instanceId;
    }

    void ScriptSystemAdvanced::DestroyScriptInstance(uint32_t instanceId) {
        auto* instance = m_InstancePool.Get(instanceId);
        if (!instance) return;

        if (instance->plugin) {
            instance->plugin->OnPlayModeExit();
            instance->plugin->Unload();
        }

        m_InstancePool.Release(instanceId);

        LNX_LOG_INFO("[ScriptSystemAdvanced] Destroyed script instance {0}", instanceId);
    }

    ScriptInstance* ScriptSystemAdvanced::GetInstance(uint32_t instanceId) {
        return m_InstancePool.Get(instanceId);
    }

    const ScriptInstance* ScriptSystemAdvanced::GetInstance(uint32_t instanceId) const {
        return m_InstancePool.Get(instanceId);
    }

    bool ScriptSystemAdvanced::CompileScript(const std::string& scriptPath, std::string& outDllPath) {
        return m_ScriptingEngine->CompileScript(scriptPath, outDllPath);
    }

    // ========================================================================
    // Hot-Reload
    // ========================================================================

    void ScriptSystemAdvanced::HotReloadAll() {
        LNX_LOG_INFO("[ScriptSystemAdvanced] Hot-reloading all scripts...");

        // 1. Serialize all states
        SerializeAllScriptStates();

        // 2. Stop all scripts
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            if (instance.IsLoaded()) {
                instance.plugin->OnPlayModeExit();
                instance.plugin->Unload();
            }
            });

        // 3. Wait for file locks to release (Windows)
#ifdef _WIN32
        Sleep(100);
#endif

        // 4. Recompile and reload
        m_InstancePool.ForEach([this](uint32_t id, ScriptInstance& instance) {
            std::string newDllPath;
            if (!m_ScriptingEngine->CompileScript(instance.sourcePath, newDllPath)) {
                LNX_LOG_ERROR("[ScriptSystemAdvanced] Hot-reload compile failed: {0}", instance.sourcePath);
                return;
            }

            instance.dllPath = newDllPath;

            // Set entity context
            auto* ctx = m_ScriptingEngine->GetEngineContext();
            ctx->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(instance.ownerEntity));

            if (!instance.plugin->Load(newDllPath, ctx)) {
                LNX_LOG_ERROR("[ScriptSystemAdvanced] Hot-reload load failed: {0}", newDllPath);
                return;
            }
            });

        // 5. Deserialize states
        DeserializeAllScriptStates();

        // 6. Call OnPlayModeEnter
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            if (instance.IsLoaded()) {
                instance.plugin->OnPlayModeEnter();
            }
            });

        LNX_LOG_INFO("[ScriptSystemAdvanced] Hot-reload complete");
    }

    void ScriptSystemAdvanced::HotReload(uint32_t instanceId) {
        auto* instance = m_InstancePool.Get(instanceId);
        if (!instance) return;

        // Serialize state
        instance->SerializeState();

        // Stop script
        if (instance->IsLoaded()) {
            instance->plugin->OnPlayModeExit();
            instance->plugin->Unload();
        }

#ifdef _WIN32
        Sleep(100);
#endif

        // Recompile
        std::string newDllPath;
        if (!m_ScriptingEngine->CompileScript(instance->sourcePath, newDllPath)) {
            LNX_LOG_ERROR("[ScriptSystemAdvanced] Hot-reload compile failed: {0}", instance->sourcePath);
            return;
        }

        instance->dllPath = newDllPath;

        // Reload
        auto* ctx = m_ScriptingEngine->GetEngineContext();
        ctx->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(instance->ownerEntity));

        if (!instance->plugin->Load(newDllPath, ctx)) {
            LNX_LOG_ERROR("[ScriptSystemAdvanced] Hot-reload load failed: {0}", newDllPath);
            return;
        }

        // Restore state
        instance->DeserializeState();

        // Restart
        instance->plugin->OnPlayModeEnter();
    }

    // ========================================================================
    // Physics Event Dispatching
    // ========================================================================

    void ScriptSystemAdvanced::DispatchCollisionEnter(entt::entity a, entt::entity b) {
        // Find scripts attached to entity a and call OnCollisionEnter
        // This would be implemented via extended Script interface
    }

    void ScriptSystemAdvanced::DispatchCollisionStay(entt::entity a, entt::entity b) {
        // Similar to DispatchCollisionEnter
    }

    void ScriptSystemAdvanced::DispatchCollisionExit(entt::entity a, entt::entity b) {
        // Similar to DispatchCollisionEnter
    }

    void ScriptSystemAdvanced::DispatchTriggerEnter(entt::entity a, entt::entity b) {
        // Similar to collision events
    }

    void ScriptSystemAdvanced::DispatchTriggerStay(entt::entity a, entt::entity b) {
        // Similar to collision events
    }

    void ScriptSystemAdvanced::DispatchTriggerExit(entt::entity a, entt::entity b) {
        // Similar to collision events
    }

    // ========================================================================
    // Profiling
    // ========================================================================

    const ScriptProfilingData* ScriptSystemAdvanced::GetProfilingData(uint32_t instanceId) const {
        auto* instance = m_InstancePool.Get(instanceId);
        return instance ? &instance->profiling : nullptr;
    }

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    void ScriptSystemAdvanced::InitializeScript(entt::entity entity, ScriptMetadata& meta) {
        // Create script instance
        // This is called when ScriptMetadata is added to an entity
    }

    void ScriptSystemAdvanced::DestroyScript(entt::entity entity, ScriptMetadata& meta) {
        // Destroy script instance
        DestroyScriptInstance(meta.instanceId);
    }

    void ScriptSystemAdvanced::UpdateScript(ScriptInstance& instance, ScriptMetadata& meta, float deltaTime) {
        if (!instance.IsLoaded()) return;
        if (!meta.IsEnabled() || meta.HasError()) return;

        PrepareScriptContext(instance, deltaTime);

        try {
            instance.plugin->Update(deltaTime);
        }
        catch (const std::exception& e) {
            LNX_LOG_ERROR("[ScriptSystemAdvanced] Script update error: {0}", e.what());
            meta.SetError(true);
        }
    }

    void ScriptSystemAdvanced::PrepareScriptContext(ScriptInstance& instance, float deltaTime) {
        auto* ctx = m_ScriptingEngine->GetEngineContext();
        if (!ctx) return;

        // Update entity reference
        ctx->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(instance.ownerEntity));
    }

    void ScriptSystemAdvanced::SerializeAllScriptStates() {
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            instance.SerializeState();
            });
    }

    void ScriptSystemAdvanced::DeserializeAllScriptStates() {
        m_InstancePool.ForEach([](uint32_t id, ScriptInstance& instance) {
            instance.DeserializeState();
            });
    }

    void ScriptSystemAdvanced::OnScriptComponentAdded(entt::registry& reg, entt::entity entity) {
        // Called when ScriptMetadata is added to an entity
        // Could auto-initialize the script here
    }

    void ScriptSystemAdvanced::OnScriptComponentRemoved(entt::registry& reg, entt::entity entity) {
        // Called when ScriptMetadata is removed from an entity
        // Note: Component is already gone at this point
    }

} // namespace Lunex
