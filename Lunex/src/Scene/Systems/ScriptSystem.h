#pragma once

/**
 * @file ScriptSystem.h
 * @brief Script system for native and C++ scripts
 * 
 * AAA Architecture: ScriptSystem lives in Scene/Systems/
 * Handles script lifecycle and execution.
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

namespace Lunex {

	// Forward declarations
	class ScriptingEngine;

	/**
	 * @class ScriptSystem
	 * @brief Scene system for script execution
	 */
	class ScriptSystem : public ISceneSystem {
	public:
		ScriptSystem();
		~ScriptSystem() override;

		// ========== ISceneSystem Interface ==========
		
		void OnAttach(SceneContext& context) override;
		void OnDetach() override;
		
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		
		void OnUpdate(Timestep ts, SceneMode mode) override;
		void OnLateUpdate(Timestep ts) override;
		
		void OnSceneEvent(const SceneSystemEvent& event) override;
		
		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Script; }
		
		bool IsActiveInMode(SceneMode mode) const override {
			return IsScriptActiveInMode(mode);
		}
		
		// ========== Script System API ==========
		
		/**
		 * @brief Get the scripting engine
		 */
		ScriptingEngine* GetScriptingEngine() const { return m_ScriptingEngine.get(); }
		
		/**
		 * @brief Reload all scripts
		 */
		void ReloadScripts();
		
	private:
		void UpdateNativeScripts(Timestep ts);
		void UpdateScriptComponents(Timestep ts);
		
	private:
		static inline std::string s_Name = "ScriptSystem";
		
		SceneContext* m_Context = nullptr;
		std::unique_ptr<ScriptingEngine> m_ScriptingEngine;
	};

} // namespace Lunex
