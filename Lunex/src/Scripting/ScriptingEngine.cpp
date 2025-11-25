#include "stpch.h"
#include "ScriptingEngine.h"
#include "Core/Input.h"
#include "Scene/Entity.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>

namespace Lunex {

	// Variables globales para el sistema de scripting (lambdas sin captura)
	static float g_CurrentDeltaTime = 0.016f;
	static Scene* g_CurrentScene = nullptr;

	ScriptingEngine::ScriptingEngine() {
	}

	ScriptingEngine::~ScriptingEngine() {
		// Limpiar scripts al destruir
		if (m_EngineContext) {
			for (auto& [uuid, plugin] : m_ScriptInstances) {
				if (plugin) {
					plugin->OnPlayModeExit();
					plugin->Unload();
				}
			}
			m_ScriptInstances.clear();
		}
	}

	void ScriptingEngine::Initialize(Scene* scene) {
		m_CurrentScene = scene;
		g_CurrentScene = scene;

		m_EngineContext = std::make_unique<EngineContext>();

		// === LOGGING ===
		m_EngineContext->LogInfo = [](const char* message) {
			LNX_LOG_INFO("[Script] {0}", message);
		};

		m_EngineContext->LogWarning = [](const char* message) {
			LNX_LOG_WARN("[Script] {0}", message);
		};

		m_EngineContext->LogError = [](const char* message) {
			LNX_LOG_ERROR("[Script] {0}", message);
		};

		// === TIME ===
		m_EngineContext->GetDeltaTime = []() -> float {
			return g_CurrentDeltaTime;
		};

		m_EngineContext->GetTime = []() -> float {
			return (float)glfwGetTime();
		};

		// === ENTITY MANAGEMENT ===
		m_EngineContext->CreateEntity = [](const char* name) -> void* {
			if (!g_CurrentScene) return nullptr;
			Entity newEntity = g_CurrentScene->CreateEntity(name);
			auto entityHandle = (entt::entity)newEntity;
			return reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));
		};

		m_EngineContext->DestroyEntity = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			g_CurrentScene->DestroyEntity(ent);
		};

		// === TRANSFORM COMPONENT ===
		m_EngineContext->GetEntityPosition = [](void* entity, Vec3* outPosition) {
			if (!entity || !g_CurrentScene || !outPosition) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outPosition->x = transform.Translation.x;
				outPosition->y = transform.Translation.y;
				outPosition->z = transform.Translation.z;
			}
		};

		m_EngineContext->SetEntityPosition = [](void* entity, const Vec3* position) {
			if (!entity || !g_CurrentScene || !position) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Translation.x = position->x;
				transform.Translation.y = position->y;
				transform.Translation.z = position->z;
			}
		};

		m_EngineContext->GetEntityRotation = [](void* entity, Vec3* outRotation) {
			if (!entity || !g_CurrentScene || !outRotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outRotation->x = transform.Rotation.x;
				outRotation->y = transform.Rotation.y;
				outRotation->z = transform.Rotation.z;
			}
		};

		m_EngineContext->SetEntityRotation = [](void* entity, const Vec3* rotation) {
			if (!entity || !g_CurrentScene || !rotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Rotation.x = rotation->x;
				transform.Rotation.y = rotation->y;
				transform.Rotation.z = rotation->z;
			}
		};

		m_EngineContext->GetEntityScale = [](void* entity, Vec3* outScale) {
			if (!entity || !g_CurrentScene || !outScale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outScale->x = transform.Scale.x;
				outScale->y = transform.Scale.y;
				outScale->z = transform.Scale.z;
			}
		};

		m_EngineContext->SetEntityScale = [](void* entity, const Vec3* scale) {
			if (!entity || !g_CurrentScene || !scale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Scale.x = scale->x;
				transform.Scale.y = scale->y;
				transform.Scale.z = scale->z;
			}
		};

		// === INPUT SYSTEM ===
		m_EngineContext->IsKeyPressed = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyDown = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyReleased = [](int keyCode) -> bool {
			return !Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsMouseButtonPressed = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonDown = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonReleased = [](int button) -> bool {
			return !Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->GetMousePosition = [](float* outX, float* outY) {
			if (!outX || !outY) return;
			auto pos = Input::GetMousePosition();
			*outX = pos.first;
			*outY = pos.second;
		};

		m_EngineContext->GetMouseX = []() -> float {
			return Input::GetMouseX();
		};

		m_EngineContext->GetMouseY = []() -> float {
			return Input::GetMouseY();
		};

		// CurrentEntity se establecerá cuando se cargue el script
		m_EngineContext->CurrentEntity = nullptr;

		for (int i = 0; i < 16; ++i) {
			m_EngineContext->reserved[i] = nullptr;
		}
	}

	void ScriptingEngine::OnScriptsStart(entt::registry& registry) {
		// Iterar por todas las entidades con ScriptComponent
		auto view = registry.view<ScriptComponent, IDComponent>();
		for (auto entity : view) {
			auto& scriptComp = view.get<ScriptComponent>(entity);
			auto& idComp = view.get<IDComponent>(entity);

			// ===== ITERAR POR TODOS LOS SCRIPTS DE LA ENTIDAD =====
			for (size_t i = 0; i < scriptComp.GetScriptCount(); i++) {
				const std::string& scriptPath = scriptComp.GetScriptPath(i);
				
				if (scriptPath.empty()) {
					continue;
				}

				// Compilar script si es necesario
				std::string dllPath;
				if (scriptComp.AutoCompile) {
					if (CompileScript(scriptPath, dllPath)) {
						// Actualizar DLL path en el componente
						if (i < scriptComp.CompiledDLLPaths.size()) {
							scriptComp.CompiledDLLPaths[i] = dllPath;
						}
						LNX_LOG_INFO("Script #{0} compiled successfully: {1}", i + 1, dllPath);
					}
					else {
						LNX_LOG_ERROR("Failed to compile script #{0}: {1}", i + 1, scriptPath);
						continue;
					}
				}

				// Si no hay DLL compilada, saltar
				if (i >= scriptComp.CompiledDLLPaths.size() || scriptComp.CompiledDLLPaths[i].empty()) {
					LNX_LOG_WARN("No compiled DLL for script #{0}: {1}", i + 1, scriptPath);
					continue;
				}

				// Cargar script
				auto plugin = std::make_unique<ScriptPlugin>();

				// Establecer CurrentEntity antes de cargar el script
				auto entityHandle = (entt::entity)entity;
				m_EngineContext->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));

				if (plugin->Load(scriptComp.CompiledDLLPaths[i], m_EngineContext.get())) {
					plugin->OnPlayModeEnter();
					
					// Actualizar estado en el componente
					if (i < scriptComp.ScriptLoadedStates.size()) {
						scriptComp.ScriptLoadedStates[i] = true;
					}
					if (i < scriptComp.ScriptPluginInstances.size()) {
						scriptComp.ScriptPluginInstances[i] = plugin.get();
					}

					// Guardar el plugin en el mapa usando UUID + índice
					uint64_t uniqueKey = ((uint64_t)idComp.ID << 32) | i;
					m_ScriptInstances[uniqueKey] = std::move(plugin);

					LNX_LOG_INFO("Script #{0} loaded and started: {1}", i + 1, scriptComp.CompiledDLLPaths[i]);
				}
				else {
					LNX_LOG_ERROR("Failed to load script #{0}: {1}", i + 1, scriptComp.CompiledDLLPaths[i]);
				}
			}
		}
	}

	void ScriptingEngine::OnScriptsStop(entt::registry& registry) {
		// Detener todos los scripts
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin) {
				plugin->OnPlayModeExit();
				plugin->Unload();
			}
		}

		// Limpiar el mapa
		m_ScriptInstances.clear();

		// Marcar todos los componentes como descargados
		auto view = registry.view<ScriptComponent>();
		for (auto entity : view) {
			auto& scriptComp = view.get<ScriptComponent>(entity);
			
			// Limpiar estados de todos los scripts
			for (size_t i = 0; i < scriptComp.ScriptLoadedStates.size(); i++) {
				scriptComp.ScriptLoadedStates[i] = false;
			}
			for (size_t i = 0; i < scriptComp.ScriptPluginInstances.size(); i++) {
				scriptComp.ScriptPluginInstances[i] = nullptr;
			}
		}
	}

	void ScriptingEngine::OnScriptsUpdate(float deltaTime) {
		// Actualizar deltaTime global
		g_CurrentDeltaTime = deltaTime;

		// Actualizar todos los scripts cargados
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin && plugin->IsLoaded()) {
				plugin->Update(deltaTime);
			}
		}
	}

	bool ScriptingEngine::CompileScript(const std::string& scriptPath, std::string& outDLLPath) {
		std::filesystem::path fullScriptPath = std::filesystem::path("assets") / scriptPath;
		std::filesystem::path scriptDir = fullScriptPath.parent_path();
		std::filesystem::path scriptName = fullScriptPath.stem();

		// Detectar configuración actual
		std::string configuration;
#ifdef LN_DEBUG
		configuration = "Debug";
#elif defined(LN_RELEASE)
		configuration = "Release";
#elif defined(LN_DIST)
		configuration = "Dist";
#else
		configuration = "Debug";
#endif

		// IMPORTANTE: La DLL debe estar en el directorio del script
		std::filesystem::path dllPath = scriptDir / "bin" / configuration / (scriptName.string() + ".dll");
		std::filesystem::path expPath = scriptDir / "bin" / configuration / (scriptName.string() + ".exp");
		std::filesystem::path libPath = scriptDir / "bin" / configuration / (scriptName.string() + ".lib");
		std::filesystem::path pdbPath = scriptDir / "bin" / configuration / (scriptName.string() + ".pdb");

		// === DESCARGAR DLL ANTIGUA SI EXISTE ===
		if (std::filesystem::exists(dllPath)) {
			auto dllTime = std::filesystem::last_write_time(dllPath);
			auto scriptTime = std::filesystem::last_write_time(fullScriptPath);

			if (dllTime >= scriptTime) {
				outDLLPath = dllPath.string();
				LNX_LOG_INFO("Found up-to-date compiled DLL: {0}", outDLLPath);
				return true;
			}

			LNX_LOG_INFO("Script has been modified, recompiling...");
			LNX_LOG_WARN("IMPORTANT: Unloading old DLL before recompilation...");

			// Descargar la DLL del plugin si está cargada
			for (auto it = m_ScriptInstances.begin(); it != m_ScriptInstances.end();) {
				// Buscar en el registry cuál entidad usa esta DLL
				bool found = false;
				auto view = m_CurrentScene->m_Registry.view<ScriptComponent, IDComponent>();
				for (auto entity : view) {
					auto& scriptComp = view.get<ScriptComponent>(entity);
					auto& idComp = view.get<IDComponent>(entity);

					// Buscar en todos los scripts del componente
					for (size_t i = 0; i < scriptComp.CompiledDLLPaths.size(); i++) {
						if (scriptComp.CompiledDLLPaths[i] == dllPath.string() && 
							i < scriptComp.ScriptLoadedStates.size() && 
							scriptComp.ScriptLoadedStates[i]) {
							
							// Construir clave única (UUID + índice)
							uint64_t uniqueKey = ((uint64_t)idComp.ID << 32) | i;
							
							if (it->first == uniqueKey) {
								LNX_LOG_INFO("Unloading script #{0} for entity: {1}", i + 1, (uint64_t)idComp.ID);
								it->second->OnPlayModeExit();
								it->second->Unload();
								it = m_ScriptInstances.erase(it);
								
								// Actualizar estado en el componente
								scriptComp.ScriptLoadedStates[i] = false;
								if (i < scriptComp.ScriptPluginInstances.size()) {
									scriptComp.ScriptPluginInstances[i] = nullptr;
								}
								
								found = true;
								break;
							}
						}
					}
					
					if (found) break;
				}
				if (!found) ++it;
			}

			// Intentar borrar DLL antigua y archivos relacionados
			std::error_code ec;
			std::filesystem::remove(dllPath, ec);
			if (ec) {
				LNX_LOG_ERROR("Failed to delete old DLL: {0} (Error: {1})", dllPath.string(), ec.message());
				LNX_LOG_ERROR("The DLL might still be in use. Try stopping Play mode first.");
				return false;
			}

			// Borrar archivos auxiliares
			std::filesystem::remove(expPath, ec);
			std::filesystem::remove(libPath, ec);
			std::filesystem::remove(pdbPath, ec);

			LNX_LOG_INFO("Old DLL and related files removed successfully");
		}

		// === DETECTAR VISUAL STUDIO AUTOMÁTICAMENTE ===
		LNX_LOG_INFO("=== Auto-detecting Visual Studio installation ===");

		std::vector<std::string> vsBasePaths = {
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Professional",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Community",
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Professional",
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Enterprise",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
		};

		std::filesystem::path vsPath;
		std::filesystem::path vcvarsPath;
		std::filesystem::path clPath;

		for (const auto& basePath : vsBasePaths) {
			std::filesystem::path candidate(basePath);
			std::filesystem::path vcvarsCandidate = candidate / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";

			if (std::filesystem::exists(vcvarsCandidate)) {
				vsPath = candidate;
				vcvarsPath = vcvarsCandidate;

				std::filesystem::path clCandidate = candidate / "VC" / "Tools" / "MSVC";
				if (std::filesystem::exists(clCandidate)) {
					for (auto& entry : std::filesystem::directory_iterator(clCandidate)) {
						if (entry.is_directory()) {
							std::filesystem::path clExe = entry.path() / "bin" / "Hostx64" / "x64" / "cl.exe";
							if (std::filesystem::exists(clExe)) {
								clPath = clExe;
								break;
							}
						}
					}
				}

				if (!clPath.empty()) {
					LNX_LOG_INFO("Found Visual Studio at: {0}", vsPath.string());
					LNX_LOG_INFO("Found cl.exe at: {0}", clPath.string());
					break;
				}
			}
		}

		if (clPath.empty()) {
			LNX_LOG_ERROR("Could not auto-detect Visual Studio installation!");
			LNX_LOG_ERROR("Please install Visual Studio 2022 or 2019 with C++ tools");
			LNX_LOG_ERROR("Download from: https://visualstudio.microsoft.com/downloads/");
			return false;
		}

		// === PREPARAR COMPILACIÓN ===
		LNX_LOG_INFO("=== Compiling script: {0} ===", scriptName.string());

		std::filesystem::path binDir = scriptDir / "bin" / configuration;
		std::filesystem::path objDir = scriptDir / "bin-int" / configuration;

		std::filesystem::create_directories(binDir);
		std::filesystem::create_directories(objDir);

		std::filesystem::path scriptCoreInclude;
		std::filesystem::path lunexInclude;
		std::filesystem::path spdlogInclude;

		std::filesystem::path searchDir = scriptDir;
		for (int i = 0; i < 10; i++) {
			std::filesystem::path candidate = searchDir / "Lunex-ScriptCore" / "src";
			if (std::filesystem::exists(candidate)) {
				scriptCoreInclude = candidate;
				lunexInclude = searchDir / "Lunex" / "src";
				spdlogInclude = searchDir / "vendor" / "spdlog" / "include";
				break;
			}
			searchDir = searchDir.parent_path();
		}

		if (scriptCoreInclude.empty()) {
			LNX_LOG_ERROR("Could not find Lunex-ScriptCore!");
			LNX_LOG_ERROR("Make sure your script is inside the Lunex project structure");
			return false;
		}

		// === CONSTRUIR COMANDO DE COMPILACIÓN ===
		std::filesystem::path tempBatPath = scriptDir / "temp_compile.bat";

		std::ofstream batFile(tempBatPath);
		if (!batFile.is_open()) {
			LNX_LOG_ERROR("Failed to create temporary batch file");
			return false;
		}

		batFile << "@echo off\n";
		batFile << "REM Auto-generated compile script\n";
		batFile << "call \"" << vcvarsPath.string() << "\" >nul 2>&1\n";
		batFile << "if errorlevel 1 (\n";
		batFile << "    echo ERROR: Failed to setup Visual Studio environment\n";
		batFile << "    exit /b 1\n";
		batFile << ")\n\n";

		std::string compilerFlags = "/LD /EHsc /std:c++20 /utf-8 /nologo";
		if (configuration == "Debug") {
			compilerFlags += " /MDd /Zi /Od /DLUNEX_SCRIPT_EXPORT /DLN_DEBUG";
		}
		else {
			compilerFlags += " /MD /O2 /DLUNEX_SCRIPT_EXPORT /DLN_RELEASE";
		}

		std::string includes =
			" /I\"" + scriptCoreInclude.string() + "\"" +
			" /I\"" + lunexInclude.string() + "\"" +
			" /I\"" + spdlogInclude.string() + "\"";

		std::string outputDll = "/Fe:\"" + dllPath.string() + "\"";
		std::string outputObj = "/Fo:\"" + objDir.string() + "\\\\\"";

		batFile << "cl.exe " << compilerFlags << includes;
		batFile << " \"" << fullScriptPath.string() << "\"";
		batFile << " " << outputDll << " " << outputObj << " 2>&1\n";
		batFile << "exit /b %errorlevel%\n";

		batFile.close();

		LNX_LOG_INFO("Compiling: {0}", scriptName.string());
		LNX_LOG_INFO("Output: {0}", dllPath.string());

		std::string batCommand = "\"" + tempBatPath.string() + "\" 2>&1";
		FILE* pipe = _popen(batCommand.c_str(), "r");
		if (!pipe) {
			LNX_LOG_ERROR("Failed to execute compile script");
			std::filesystem::remove(tempBatPath);
			return false;
		}

		char buffer[512];
		bool hadErrors = false;
		while (fgets(buffer, sizeof(buffer), pipe)) {
			std::string line = buffer;
			if (!line.empty() && line.back() == '\n') line.pop_back();
			if (!line.empty()) {
				if (line.find("error") != std::string::npos ||
					line.find("Error") != std::string::npos ||
					line.find("ERROR") != std::string::npos) {
					LNX_LOG_ERROR("[Compiler] {0}", line);
					hadErrors = true;
				}
				else if (line.find("warning") != std::string::npos ||
					line.find("Warning") != std::string::npos) {
					LNX_LOG_WARN("[Compiler] {0}", line);
				}
				else if (line.find("Creating library") == std::string::npos &&
					line.find(".exp") == std::string::npos) {
					LNX_LOG_INFO("[Compiler] {0}", line);
				}
			}
		}

		int result = _pclose(pipe);
		std::filesystem::remove(tempBatPath);

		if (result != 0 || hadErrors) {
			LNX_LOG_ERROR("Compilation failed with error code: {0}", result);
			return false;
		}

		if (std::filesystem::exists(dllPath)) {
			outDLLPath = dllPath.string();
			LNX_LOG_INFO("=== Script compiled successfully! ===");
			LNX_LOG_INFO("DLL created at: {0}", outDLLPath);
			return true;
		}
		else {
			LNX_LOG_ERROR("Compilation succeeded but DLL not found at: {0}", dllPath.string());
			return false;
		}
	}

}
