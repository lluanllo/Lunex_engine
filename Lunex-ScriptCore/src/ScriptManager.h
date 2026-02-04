#pragma once

/**
 * @file ScriptManager.h
 * @brief AAA Architecture: Example of how to integrate the scripting system
 * 
 * This file demonstrates how to use the ScriptSystem in your EditorLayer
 * or game code for loading and executing dynamic scripts.
 * 
 * The new AAA architecture provides:
 * - Hot-reload support with state preservation
 * - Data-oriented batch processing
 * - Reflection system for editor integration
 * - Physics event callbacks
 * - Profiling and debugging tools
 */

#include "ScriptPlugin.h"
#include "ScriptContext.h"
#include "LunexScriptingAPI.h"
#include <string>
#include <memory>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lunex {

    /**
     * @class ScriptManager
     * @brief Simple manager for loading and running scripts
     * 
     * This is a standalone manager for basic script usage.
     * For full integration with the ECS, use ScriptSystem instead.
     */
    class ScriptManager
    {
    public:
        ScriptManager()
        {
            InitializeEngineContext();
        }

        ~ScriptManager()
        {
            UnloadCurrentScript();
        }

        /**
         * @brief Load a script from a compiled DLL
         * @param dllPath Path to the compiled .dll file
         * @return True if loaded successfully
         */
        bool LoadScript(const std::string& dllPath)
        {
            // If a script is already loaded, unload it first
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                UnloadCurrentScript();
            }

            // Create new plugin
            m_CurrentScript = std::make_unique<ScriptPlugin>();

            // Attempt to load
            if (!m_CurrentScript->Load(dllPath, &m_EngineContext))
            {
                LogError("Failed to load script: " + dllPath);
                m_CurrentScript.reset();
                return false;
            }

            LogInfo("Script loaded successfully: " + dllPath);
            return true;
        }

        /**
         * @brief Unload the current script
         */
        void UnloadCurrentScript()
        {
            if (m_CurrentScript)
            {
                m_CurrentScript->Unload();
                m_CurrentScript.reset();
                LogInfo("Script unloaded");
            }
        }

        /**
         * @brief Hot-reload the current script
         * @return True if reload was successful
         */
        bool ReloadCurrentScript()
        {
            if (!m_CurrentScript || !m_CurrentScript->IsLoaded())
            {
                LogWarning("No script loaded to reload");
                return false;
            }

            std::string currentPath = m_CurrentScript->GetPath();
            UnloadCurrentScript();

            // Small delay to ensure Windows releases file lock
            #ifdef _WIN32
            Sleep(100);
            #endif

            return LoadScript(currentPath);
        }

        /**
         * @brief Update the script (call in game loop)
         * @param deltaTime Frame delta time
         */
        void Update(float deltaTime)
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded() && m_IsPlaying)
            {
                m_CurrentScript->Update(deltaTime);
            }
        }

        /**
         * @brief Render callback (if script has rendering)
         */
        void Render()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded() && m_IsPlaying)
            {
                m_CurrentScript->Render();
            }
        }

        /**
         * @brief Enter play mode
         */
        void EnterPlayMode()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                m_IsPlaying = true;
                m_CurrentScript->OnPlayModeEnter();
                LogInfo("Entering Play mode");
            }
        }

        /**
         * @brief Exit play mode
         */
        void ExitPlayMode()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                m_CurrentScript->OnPlayModeExit();
                m_IsPlaying = false;
                LogInfo("Exiting Play mode");
            }
        }

        bool IsPlaying() const { return m_IsPlaying; }
        bool HasScriptLoaded() const { return m_CurrentScript && m_CurrentScript->IsLoaded(); }
        
        /**
         * @brief Get the engine context for advanced usage
         */
        EngineContext* GetEngineContext() { return &m_EngineContext; }

    private:
        void InitializeEngineContext()
        {
            // === LOGGING ===
            m_EngineContext.LogInfo = [](const char* message) {
                std::cout << "[Script Info] " << message << "\n";
            };

            m_EngineContext.LogWarning = [](const char* message) {
                std::cout << "[Script Warning] " << message << "\n";
            };

            m_EngineContext.LogError = [](const char* message) {
                std::cerr << "[Script Error] " << message << "\n";
            };

            // === TIME ===
            m_EngineContext.GetDeltaTime = []() -> float {
                return 0.016f; // Placeholder - replace with actual time
            };

            m_EngineContext.GetTime = []() -> float {
                return 0.0f; // Placeholder - replace with actual time
            };

            // === ENTITY MANAGEMENT ===
            m_EngineContext.CreateEntity = [](const char* name) -> void* {
                // Placeholder - integrate with your ECS
                return nullptr;
            };

            m_EngineContext.DestroyEntity = [](void* entity) {
                // Placeholder - integrate with your ECS
            };

            // Initialize all other callbacks to nullptr
            // Transform
            m_EngineContext.GetEntityPosition = nullptr;
            m_EngineContext.SetEntityPosition = nullptr;
            m_EngineContext.GetEntityRotation = nullptr;
            m_EngineContext.SetEntityRotation = nullptr;
            m_EngineContext.GetEntityScale = nullptr;
            m_EngineContext.SetEntityScale = nullptr;
            
            // Input
            m_EngineContext.IsKeyPressed = nullptr;
            m_EngineContext.IsKeyDown = nullptr;
            m_EngineContext.IsKeyReleased = nullptr;
            m_EngineContext.IsMouseButtonPressed = nullptr;
            m_EngineContext.IsMouseButtonDown = nullptr;
            m_EngineContext.IsMouseButtonReleased = nullptr;
            m_EngineContext.GetMousePosition = nullptr;
            m_EngineContext.GetMouseX = nullptr;
            m_EngineContext.GetMouseY = nullptr;
            
            // Rigidbody2D
            m_EngineContext.HasRigidbody2D = nullptr;
            m_EngineContext.GetLinearVelocity = nullptr;
            m_EngineContext.SetLinearVelocity = nullptr;
            m_EngineContext.ApplyLinearImpulse = nullptr;
            m_EngineContext.ApplyLinearImpulseToCenter = nullptr;
            m_EngineContext.ApplyForce = nullptr;
            m_EngineContext.ApplyForceToCenter = nullptr;
            m_EngineContext.GetMass = nullptr;
            m_EngineContext.GetGravityScale = nullptr;
            m_EngineContext.SetGravityScale = nullptr;
            
            // Rigidbody3D
            m_EngineContext.HasRigidbody3D = nullptr;
            m_EngineContext.GetLinearVelocity3D = nullptr;
            m_EngineContext.SetLinearVelocity3D = nullptr;
            m_EngineContext.GetAngularVelocity3D = nullptr;
            m_EngineContext.SetAngularVelocity3D = nullptr;
            m_EngineContext.ApplyForce3D = nullptr;
            m_EngineContext.ApplyForceAtPoint3D = nullptr;
            m_EngineContext.ApplyImpulse3D = nullptr;
            m_EngineContext.ApplyImpulseAtPoint3D = nullptr;
            m_EngineContext.ApplyTorque3D = nullptr;
            m_EngineContext.ApplyTorqueImpulse3D = nullptr;
            m_EngineContext.GetMass3D = nullptr;
            m_EngineContext.SetMass3D = nullptr;
            m_EngineContext.GetFriction3D = nullptr;
            m_EngineContext.SetFriction3D = nullptr;
            m_EngineContext.GetRestitution3D = nullptr;
            m_EngineContext.SetRestitution3D = nullptr;
            m_EngineContext.GetLinearDamping3D = nullptr;
            m_EngineContext.SetLinearDamping3D = nullptr;
            m_EngineContext.GetAngularDamping3D = nullptr;
            m_EngineContext.SetAngularDamping3D = nullptr;
            m_EngineContext.SetLinearFactor3D = nullptr;
            m_EngineContext.GetLinearFactor3D = nullptr;
            m_EngineContext.SetAngularFactor3D = nullptr;
            m_EngineContext.GetAngularFactor3D = nullptr;
            m_EngineContext.ClearForces3D = nullptr;
            m_EngineContext.Activate3D = nullptr;
            m_EngineContext.Deactivate3D = nullptr;
            m_EngineContext.IsActive3D = nullptr;
            m_EngineContext.IsKinematic3D = nullptr;
            m_EngineContext.SetKinematic3D = nullptr;

            // Entity handle
            m_EngineContext.CurrentEntity = nullptr;

            // Reserved fields
            for (int i = 0; i < 16; ++i)
            {
                m_EngineContext.reserved[i] = nullptr;
            }
        }

        void LogInfo(const std::string& message)
        {
            if (m_EngineContext.LogInfo)
                m_EngineContext.LogInfo(message.c_str());
        }

        void LogWarning(const std::string& message)
        {
            if (m_EngineContext.LogWarning)
                m_EngineContext.LogWarning(message.c_str());
        }

        void LogError(const std::string& message)
        {
            if (m_EngineContext.LogError)
                m_EngineContext.LogError(message.c_str());
        }

    private:
        std::unique_ptr<ScriptPlugin> m_CurrentScript;
        EngineContext m_EngineContext;
        bool m_IsPlaying = false;
    };

} // namespace Lunex

/* ============================================================================
   AAA ARCHITECTURE: USAGE EXAMPLES
   ============================================================================

// Example 1: Simple script usage with ScriptManager
// -------------------------------------------------

class EditorLayer : public Layer
{
    ScriptManager m_ScriptManager;

    void OnUpdate(Timestep ts) override
    {
        m_ScriptManager.Update(ts);
    }

    void LoadPlayerScript()
    {
        // Load compiled script DLL
        m_ScriptManager.LoadScript("bin/Debug/Scripts/PlayerController.dll");
        m_ScriptManager.EnterPlayMode();
    }
};

// Example 2: Full ECS integration with ScriptSystem
// -------------------------------------------------

// In Scene::InitializeSystems():
void Scene::InitializeSystems()
{
    // Create and register the script system
    auto scriptSystem = std::make_unique<ScriptSystem>();
    scriptSystem->OnAttach(m_Context);
    m_Systems.push_back(std::move(scriptSystem));
}

// In Scene::OnRuntimeStart():
void Scene::OnRuntimeStart()
{
    auto* scriptSystem = GetSystem<ScriptSystem>();
    if (scriptSystem) {
        scriptSystem->OnRuntimeStart(SceneMode::Play);
    }
}

// Example 3: Creating a script for an entity
// ------------------------------------------

void CreatePlayerEntity(Scene& scene)
{
    Entity player = scene.CreateEntity("Player");
    player.AddComponent<TransformComponent>();
    player.AddComponent<MeshComponent>(ModelType::Cube);
    
    // Add script component
    auto& script = player.AddComponent<ScriptComponent>("Scripts/PlayerController.cpp");
    script.AutoCompile = true;  // Compile on Play
}

// Example 4: User script implementation
// -------------------------------------

// In PlayerController.cpp:
#include "LunexScriptingAPI.h"

class PlayerController : public Lunex::Script
{
public:
    // Public variables (visible in editor)
    float speed = 5.0f;
    float jumpForce = 10.0f;

protected:
    void Start() override
    {
        // Register public variables for editor
        RegisterVar("Speed", &speed);
        RegisterVarRange("Jump Force", &jumpForce, 0.0f, 50.0f);
        
        debug.Log("PlayerController initialized!");
    }

    void Update() override
    {
        // Move player
        Vec3 pos = transform.GetPosition();
        
        if (input.IsKeyPressed(LNX_KEY_W))
            pos.z -= speed * time.DeltaTime();
        if (input.IsKeyPressed(LNX_KEY_S))
            pos.z += speed * time.DeltaTime();
        if (input.IsKeyPressed(LNX_KEY_A))
            pos.x -= speed * time.DeltaTime();
        if (input.IsKeyPressed(LNX_KEY_D))
            pos.x += speed * time.DeltaTime();
            
        transform.SetPosition(pos);
        
        // Jump (3D physics)
        if (input.IsKeyPressed(LNX_KEY_SPACE) && rigidbody3d.Exists())
        {
            rigidbody3d.ApplyImpulse(Vec3(0, jumpForce, 0));
        }
    }
};

// Export functions (required for DLL loading)
extern "C" {
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion() {
        return Lunex::SCRIPTING_API_VERSION;
    }
    
    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule() {
        return new PlayerController();
    }
    
    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module) {
        delete module;
    }
}

   ============================================================================
*/
