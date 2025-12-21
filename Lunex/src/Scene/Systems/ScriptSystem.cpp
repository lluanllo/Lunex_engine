#include "stpch.h"
#include "ScriptSystem.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/ScriptableEntity.h"
#include "Scripting/ScriptingEngine.h"

namespace Lunex {

	ScriptSystem::ScriptSystem() {
		m_ScriptingEngine = std::make_unique<ScriptingEngine>();
	}

	ScriptSystem::~ScriptSystem() {
		// ScriptingEngine cleaned up by unique_ptr
	}

	// ========================================================================
	// ISceneSystem Interface
	// ========================================================================

	void ScriptSystem::OnAttach(SceneContext& context) {
		m_Context = &context;
		
		// Initialize scripting engine with the owning scene
		m_ScriptingEngine->Initialize(context.OwningScene);
		
		LNX_LOG_INFO("ScriptSystem attached");
	}

	void ScriptSystem::OnDetach() {
		m_Context = nullptr;
		LNX_LOG_INFO("ScriptSystem detached");
	}

	void ScriptSystem::OnRuntimeStart(SceneMode mode) {
		if (!IsActiveInMode(mode)) return;
		
		// Start scripts
		if (m_Context && m_Context->Registry) {
			m_ScriptingEngine->OnScriptsStart(*m_Context->Registry);
		}
		
		LNX_LOG_INFO("ScriptSystem started (mode: {0})", SceneModeToString(mode));
	}

	void ScriptSystem::OnRuntimeStop() {
		// Stop scripts
		if (m_Context && m_Context->Registry) {
			m_ScriptingEngine->OnScriptsStop(*m_Context->Registry);
		}
		
		LNX_LOG_INFO("ScriptSystem stopped");
	}

	void ScriptSystem::OnUpdate(Timestep ts, SceneMode mode) {
		if (!IsActiveInMode(mode)) return;
		
		// Update C++ scripting engine
		m_ScriptingEngine->OnScriptsUpdate(ts);
		
		// Update native scripts
		UpdateNativeScripts(ts);
	}

	void ScriptSystem::OnLateUpdate(Timestep ts) {
		// Late update for scripts that need it
	}

	void ScriptSystem::OnSceneEvent(const SceneSystemEvent& event) {
		switch (event.Type) {
			case SceneEventType::EntityDestroyed: {
				// Cleanup script instances for destroyed entity
				const auto* data = event.GetData<EntityEventData>();
				if (data) {
					// Could notify scripting engine here
				}
				break;
			}
			default:
				break;
		}
	}

	// ========================================================================
	// Script System API
	// ========================================================================

	void ScriptSystem::ReloadScripts() {
		if (m_Context && m_Context->Registry) {
			// Re-initialize scripting engine
			m_ScriptingEngine->Initialize(m_Context->OwningScene);
		}
	}

	// ========================================================================
	// Internal Methods
	// ========================================================================

	void ScriptSystem::UpdateNativeScripts(Timestep ts) {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<NativeScriptComponent>();
		for (auto entity : view) {
			auto& nsc = view.get<NativeScriptComponent>(entity);
			
			if (!nsc.Instance) {
				nsc.Instance = nsc.InstantiateScript();
				nsc.Instance->m_Entity = Entity{ entity, m_Context->OwningScene };
				nsc.Instance->OnCreate();
			}
			nsc.Instance->OnUpdate(ts);
		}
	}

	void ScriptSystem::UpdateScriptComponents(Timestep ts) {
		// ScriptComponent updates are handled by ScriptingEngine
	}

} // namespace Lunex
