#pragma once

#include "Core/Core.h"
#include "Core/JobSystem/JobSystem.h"  // ? Added for parallel compilation
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include "../../Lunex-ScriptCore/src/ScriptPlugin.h"

#include <entt.hpp>
#include <unordered_map>
#include <memory>
#include <string>

namespace Lunex {

	class ScriptingEngine {
	public:
		ScriptingEngine();
		~ScriptingEngine();

		// Inicializar el contexto de scripting
		void Initialize(Scene* scene);

		// Lifecycle del sistema de scripts
		void OnScriptsStart(entt::registry& registry);
		void OnScriptsStop(entt::registry& registry);
		void OnScriptsUpdate(float deltaTime);

		// Compilación de scripts
		bool CompileScript(const std::string& scriptPath, std::string& outDLLPath, bool forceRecompile = false);

		// Obtener el contexto del engine
		EngineContext* GetEngineContext() { return m_EngineContext.get(); }

	private:
		// Contexto del engine para scripts
		std::unique_ptr<EngineContext> m_EngineContext;

		// Mapa de instancias de scripts (UUID -> ScriptPlugin)
		std::unordered_map<UUID, std::unique_ptr<ScriptPlugin>> m_ScriptInstances;

		// Puntero a la scene actual
		Scene* m_CurrentScene = nullptr;
	};

}
