#pragma once

// EJEMPLO: Cómo integrar el sistema de scripting en el Editor
// 
// Este archivo muestra cómo usar ScriptPlugin en tu EditorLayer o donde necesites
// cargar y ejecutar scripts dinámicos.

#include "ScriptPlugin.h"
#include "LunexScriptingAPI.h"
#include <string>
#include <memory>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lunex {

    // Ejemplo de clase que gestiona scripts en el editor
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

        // Cargar un script desde archivo
        bool LoadScript(const std::string& dllPath)
        {
            // Si hay un script cargado, descargarlo primero
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                UnloadCurrentScript();
            }

            // Crear nuevo plugin
            m_CurrentScript = std::make_unique<ScriptPlugin>();

            // Intentar cargar
            if (!m_CurrentScript->Load(dllPath, &m_EngineContext))
            {
                LogError("Error al cargar el script: " + dllPath);
                m_CurrentScript.reset();
                return false;
            }

            LogInfo("Script cargado exitosamente: " + dllPath);
            return true;
        }

        // Descargar el script actual
        void UnloadCurrentScript()
        {
            if (m_CurrentScript)
            {
                m_CurrentScript->Unload();
                m_CurrentScript.reset();
                LogInfo("Script descargado");
            }
        }

        // Recargar el script actual (hot-reload)
        bool ReloadCurrentScript()
        {
            if (!m_CurrentScript || !m_CurrentScript->IsLoaded())
            {
                LogWarning("No hay script cargado para recargar");
                return false;
            }

            std::string currentPath = m_CurrentScript->GetPath();
            UnloadCurrentScript();

            // Pequeño delay para asegurar que Windows suelta el lock del archivo
            #ifdef _WIN32
            Sleep(100);
            #endif

            return LoadScript(currentPath);
        }

        // Llamar en el loop de Update del editor
        void Update(float deltaTime)
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded() && m_IsPlaying)
            {
                m_CurrentScript->Update(deltaTime);
            }
        }

        // Llamar en el loop de Render del editor
        void Render()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded() && m_IsPlaying)
            {
                m_CurrentScript->Render();
            }
        }

        // Entrar en modo Play
        void EnterPlayMode()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                m_IsPlaying = true;
                m_CurrentScript->OnPlayModeEnter();
                LogInfo("Entrando en modo Play");
            }
        }

        // Salir del modo Play
        void ExitPlayMode()
        {
            if (m_CurrentScript && m_CurrentScript->IsLoaded())
            {
                m_CurrentScript->OnPlayModeExit();
                m_IsPlaying = false;
                LogInfo("Saliendo del modo Play");
            }
        }

        bool IsPlaying() const { return m_IsPlaying; }
        bool HasScriptLoaded() const { return m_CurrentScript && m_CurrentScript->IsLoaded(); }

    private:
        void InitializeEngineContext()
        {
            // Configurar callbacks de logging
            m_EngineContext.LogInfo = [](const char* message) {
                // Aquí puedes integrar con tu sistema de logging del motor
                // Por ejemplo: LN_CORE_INFO(message);
                std::cout << "[Script Info] " << message << "\n";
            };

            m_EngineContext.LogWarning = [](const char* message) {
                // LN_CORE_WARN(message);
                std::cout << "[Script Warning] " << message << "\n";
            };

            m_EngineContext.LogError = [](const char* message) {
                // LN_CORE_ERROR(message);
                std::cerr << "[Script Error] " << message << "\n";
            };

            // Configurar callbacks de tiempo
            m_EngineContext.GetDeltaTime = []() -> float {
                // Aquí deberías obtener el deltaTime real de tu motor
                // Por ejemplo: return Time::GetDeltaTime();
                return 0.016f; // Placeholder
            };

            m_EngineContext.GetTime = []() -> float {
                // Aquí deberías obtener el tiempo total de tu motor
                // Por ejemplo: return Time::GetTime();
                return 0.0f; // Placeholder
            };

            // Configurar callbacks de entidades (ejemplo)
            m_EngineContext.CreateEntity = [](const char* name) -> void* {
                // Aquí deberías crear una entidad en tu sistema ECS
                // Por ejemplo: return (void*)m_ActiveScene->CreateEntity(name);
                return nullptr; // Placeholder
            };

            m_EngineContext.DestroyEntity = [](void* entity) {
                // Aquí deberías destruir la entidad
                // Por ejemplo: m_ActiveScene->DestroyEntity((Entity*)entity);
            };

            // Inicializar campos reservados a nullptr
            for (int i = 0; i < 32; ++i)
            {
                m_EngineContext.reserved[i] = nullptr;
            }
        }

        void LogInfo(const std::string& message)
        {
            m_EngineContext.LogInfo(message.c_str());
        }

        void LogWarning(const std::string& message)
        {
            m_EngineContext.LogWarning(message.c_str());
        }

        void LogError(const std::string& message)
        {
            m_EngineContext.LogError(message.c_str());
        }

    private:
        std::unique_ptr<ScriptPlugin> m_CurrentScript;
        EngineContext m_EngineContext;
        bool m_IsPlaying = false;
    };

} // namespace Lunex

/* ============================================================================
   EJEMPLO DE USO EN EDITORLAYER.H
   ============================================================================

class EditorLayer : public Layer
{
public:
    // ...existing code...

    void OnUpdate(Timestep ts) override
    {
        // ...existing code...
        
        // Actualizar script si está en modo Play
        m_ScriptManager.Update(ts);
    }

    void OnImGuiRender() override
    {
        // ...existing code...
        
        // Panel de scripts (ejemplo)
        if (ImGui::Begin("Scripts"))
        {
            if (ImGui::Button("Cargar Script"))
            {
                // Aquí puedes abrir un file dialog
                std::string path = "bin/Debug-windows-x86_64/Scripts/ExampleScript/ExampleScript.dll";
                m_ScriptManager.LoadScript(path);
            }
            
            if (m_ScriptManager.HasScriptLoaded())
            {
                if (ImGui::Button("Recargar Script (Hot-Reload)"))
                {
                    m_ScriptManager.ReloadCurrentScript();
                }
                
                ImGui::Separator();
                
                if (!m_ScriptManager.IsPlaying())
                {
                    if (ImGui::Button("? Play"))
                    {
                        m_ScriptManager.EnterPlayMode();
                    }
                }
                else
                {
                    if (ImGui::Button("? Stop"))
                    {
                        m_ScriptManager.ExitPlayMode();
                    }
                }
            }
        }
        ImGui::End();
    }

private:
    ScriptManager m_ScriptManager;
};

   ============================================================================
*/
