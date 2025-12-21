#include "stpch.h"
#include "SceneManager.h"

#include "Scene.h"
#include "SceneSerializer.h"
#include "Scene/Camera/EditorCamera.h"

namespace Lunex {

	SceneManager* SceneManager::s_Instance = nullptr;

	SceneManager::SceneManager() {
		LNX_CORE_ASSERT(!s_Instance, "SceneManager already exists!");
		s_Instance = this;
	}

	SceneManager::~SceneManager() {
		Shutdown();
		s_Instance = nullptr;
	}

	// ========================================================================
	// Lifecycle
	// ========================================================================

	void SceneManager::Initialize() {
		LNX_LOG_INFO("SceneManager initialized");
	}

	void SceneManager::Shutdown() {
		// Stop runtime if active
		if (m_Mode != SceneMode::Edit) {
			Stop();
		}
		
		// Clear all scenes
		m_LoadedScenes.clear();
		m_ActiveScene = nullptr;
		m_EditorScene = nullptr;
		
		// Clear stacks
		while (!m_BackStack.empty()) m_BackStack.pop();
		while (!m_ForwardStack.empty()) m_ForwardStack.pop();
		
		LNX_LOG_INFO("SceneManager shutdown");
	}

	// ========================================================================
	// Scene Loading
	// ========================================================================

	Ref<Scene> SceneManager::CreateScene(const std::string& name) {
		Ref<Scene> scene = CreateRef<Scene>();
		
		// Set viewport size if we have one
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
			scene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
		}
		
		SetActiveScene(scene);
		return scene;
	}

	bool SceneManager::LoadScene(const std::filesystem::path& path, const SceneTransition& transition) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("Scene file not found: {0}", path.string());
			return false;
		}
		
		// Stop current runtime if active
		if (m_Mode != SceneMode::Edit) {
			Stop();
		}
		
		// Push current scene to back stack
		if (!m_ActiveScenePath.empty()) {
			m_BackStack.push(m_ActiveScenePath);
			// Clear forward stack on new navigation
			while (!m_ForwardStack.empty()) m_ForwardStack.pop();
		}
		
		// Start transition
		if (transition.Type != SceneTransitionType::Instant) {
			m_IsTransitioning = true;
			m_CurrentTransition = transition;
			m_TransitionProgress = 0.0f;
			
			if (transition.OnTransitionStart) {
				transition.OnTransitionStart();
			}
		}
		
		// Unload additive scenes (unless persistent)
		for (auto it = m_LoadedScenes.begin(); it != m_LoadedScenes.end();) {
			if (it->IsAdditive && !it->IsPersistent) {
				it = m_LoadedScenes.erase(it);
			}
			else {
				++it;
			}
		}
		
		// Load new scene
		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		
		if (!serializer.Deserialize(path.string())) {
			LNX_LOG_ERROR("Failed to deserialize scene: {0}", path.string());
			return false;
		}
		
		// Set viewport size
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
			newScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
		}
		
		SetActiveScene(newScene, path);
		
		// Complete transition
		if (transition.Type != SceneTransitionType::Instant) {
			m_IsTransitioning = false;
			if (transition.OnTransitionComplete) {
				transition.OnTransitionComplete();
			}
		}
		
		LNX_LOG_INFO("Loaded scene: {0}", path.filename().string());
		return true;
	}

	bool SceneManager::LoadSceneAdditive(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("Additive scene file not found: {0}", path.string());
			return false;
		}
		
		// Load scene
		Ref<Scene> additiveScene = CreateRef<Scene>();
		SceneSerializer serializer(additiveScene);
		
		if (!serializer.Deserialize(path.string())) {
			LNX_LOG_ERROR("Failed to deserialize additive scene: {0}", path.string());
			return false;
		}
		
		// Set viewport size
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
			additiveScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
		}
		
		// Add to loaded scenes
		SceneEntry entry;
		entry.SceneInstance = additiveScene;
		entry.FilePath = path;
		entry.SceneID = UUID();
		entry.IsAdditive = true;
		entry.IsActive = true;
		m_LoadedScenes.push_back(entry);
		
		// If in runtime mode, start the additive scene
		if (m_Mode == SceneMode::Play) {
			additiveScene->OnRuntimeStart();
		}
		else if (m_Mode == SceneMode::Simulate) {
			additiveScene->OnSimulationStart();
		}
		
		LNX_LOG_INFO("Loaded additive scene: {0}", path.filename().string());
		return true;
	}

	void SceneManager::UnloadAdditiveScene(UUID sceneID) {
		for (auto it = m_LoadedScenes.begin(); it != m_LoadedScenes.end(); ++it) {
			if (it->SceneID == sceneID && it->IsAdditive) {
				// Stop if running
				if (m_Mode == SceneMode::Play) {
					it->SceneInstance->OnRuntimeStop();
				}
				else if (m_Mode == SceneMode::Simulate) {
					it->SceneInstance->OnSimulationStop();
				}
				
				m_LoadedScenes.erase(it);
				LNX_LOG_INFO("Unloaded additive scene: {0}", (uint64_t)sceneID);
				return;
			}
		}
	}

	void SceneManager::UnloadAllAdditiveScenes() {
		for (auto it = m_LoadedScenes.begin(); it != m_LoadedScenes.end();) {
			if (it->IsAdditive && !it->IsPersistent) {
				if (m_Mode == SceneMode::Play) {
					it->SceneInstance->OnRuntimeStop();
				}
				else if (m_Mode == SceneMode::Simulate) {
					it->SceneInstance->OnSimulationStop();
				}
				it = m_LoadedScenes.erase(it);
			}
			else {
				++it;
			}
		}
	}

	// ========================================================================
	// Scene Access
	// ========================================================================

	Ref<Scene> SceneManager::GetScene(UUID sceneID) const {
		for (const auto& entry : m_LoadedScenes) {
			if (entry.SceneID == sceneID) {
				return entry.SceneInstance;
			}
		}
		return nullptr;
	}

	// ========================================================================
	// Scene Mode
	// ========================================================================

	void SceneManager::StartPlay() {
		if (m_Mode != SceneMode::Edit) {
			Stop();
		}
		
		SceneMode oldMode = m_Mode;
		m_Mode = SceneMode::Play;
		
		// Copy editor scene for restoration
		m_EditorScene = m_ActiveScene;
		m_ActiveScene = CopySceneForRuntime(m_EditorScene);
		
		// Start runtime
		m_ActiveScene->OnRuntimeStart();
		
		// Start additive scenes
		for (auto& entry : m_LoadedScenes) {
			if (entry.IsAdditive && entry.IsActive) {
				entry.SceneInstance->OnRuntimeStart();
			}
		}
		
		NotifyModeChanged(oldMode, m_Mode);
		LNX_LOG_INFO("Scene started: Play mode");
	}

	void SceneManager::StartSimulate() {
		if (m_Mode != SceneMode::Edit) {
			Stop();
		}
		
		SceneMode oldMode = m_Mode;
		m_Mode = SceneMode::Simulate;
		
		// Copy editor scene for restoration
		m_EditorScene = m_ActiveScene;
		m_ActiveScene = CopySceneForRuntime(m_EditorScene);
		
		// Start simulation
		m_ActiveScene->OnSimulationStart();
		
		// Start additive scenes
		for (auto& entry : m_LoadedScenes) {
			if (entry.IsAdditive && entry.IsActive) {
				entry.SceneInstance->OnSimulationStart();
			}
		}
		
		NotifyModeChanged(oldMode, m_Mode);
		LNX_LOG_INFO("Scene started: Simulate mode");
	}

	void SceneManager::Stop() {
		if (m_Mode == SceneMode::Edit) return;
		
		SceneMode oldMode = m_Mode;
		
		// Stop runtime
		if (m_Mode == SceneMode::Play) {
			m_ActiveScene->OnRuntimeStop();
			for (auto& entry : m_LoadedScenes) {
				if (entry.IsAdditive && entry.IsActive) {
					entry.SceneInstance->OnRuntimeStop();
				}
			}
		}
		else if (m_Mode == SceneMode::Simulate) {
			m_ActiveScene->OnSimulationStop();
			for (auto& entry : m_LoadedScenes) {
				if (entry.IsAdditive && entry.IsActive) {
					entry.SceneInstance->OnSimulationStop();
				}
			}
		}
		
		// Restore editor scene
		m_ActiveScene = m_EditorScene;
		m_EditorScene = nullptr;
		m_Mode = SceneMode::Edit;
		
		NotifyModeChanged(oldMode, m_Mode);
		LNX_LOG_INFO("Scene stopped: Edit mode");
	}

	void SceneManager::Pause() {
		if (m_Mode != SceneMode::Play && m_Mode != SceneMode::Simulate) return;
		
		SceneMode oldMode = m_Mode;
		m_Mode = SceneMode::Paused;
		
		NotifyModeChanged(oldMode, m_Mode);
		LNX_LOG_INFO("Scene paused");
	}

	void SceneManager::Resume() {
		// TODO: Track previous mode before pause
		if (m_Mode != SceneMode::Paused) return;
		
		SceneMode oldMode = m_Mode;
		m_Mode = SceneMode::Play;  // Could be Simulate, need to track
		
		NotifyModeChanged(oldMode, m_Mode);
		LNX_LOG_INFO("Scene resumed");
	}

	// ========================================================================
	// Update
	// ========================================================================

	void SceneManager::OnUpdate(Timestep ts, EditorCamera* editorCamera) {
		if (!m_ActiveScene) return;
		
		switch (m_Mode) {
			case SceneMode::Edit:
				if (editorCamera) {
					m_ActiveScene->OnUpdateEditor(ts, *editorCamera);
				}
				break;
				
			case SceneMode::Simulate:
				if (editorCamera) {
					m_ActiveScene->OnUpdateSimulation(ts, *editorCamera);
				}
				break;
				
			case SceneMode::Play:
				m_ActiveScene->OnUpdateRuntime(ts);
				break;
				
			case SceneMode::Paused:
				// No updates while paused
				break;
		}
		
		// Update additive scenes
		for (auto& entry : m_LoadedScenes) {
			if (entry.IsAdditive && entry.IsActive) {
				switch (m_Mode) {
					case SceneMode::Edit:
						if (editorCamera) {
							entry.SceneInstance->OnUpdateEditor(ts, *editorCamera);
						}
						break;
					case SceneMode::Simulate:
						if (editorCamera) {
							entry.SceneInstance->OnUpdateSimulation(ts, *editorCamera);
						}
						break;
					case SceneMode::Play:
						entry.SceneInstance->OnUpdateRuntime(ts);
						break;
					case SceneMode::Paused:
						break;
				}
			}
		}
	}

	void SceneManager::OnViewportResize(uint32_t width, uint32_t height) {
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		
		if (m_ActiveScene) {
			m_ActiveScene->OnViewportResize(width, height);
		}
		
		for (auto& entry : m_LoadedScenes) {
			if (entry.IsAdditive) {
				entry.SceneInstance->OnViewportResize(width, height);
			}
		}
	}

	// ========================================================================
	// Navigation
	// ========================================================================

	bool SceneManager::NavigateBack() {
		if (m_BackStack.empty()) return false;
		
		// Push current to forward stack
		if (!m_ActiveScenePath.empty()) {
			m_ForwardStack.push(m_ActiveScenePath);
		}
		
		// Pop from back stack and load
		std::filesystem::path path = m_BackStack.top();
		m_BackStack.pop();
		
		// Load without pushing to back stack
		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		
		if (serializer.Deserialize(path.string())) {
			if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
				newScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
			}
			SetActiveScene(newScene, path);
			return true;
		}
		
		return false;
	}

	bool SceneManager::NavigateForward() {
		if (m_ForwardStack.empty()) return false;
		
		// Push current to back stack
		if (!m_ActiveScenePath.empty()) {
			m_BackStack.push(m_ActiveScenePath);
		}
		
		// Pop from forward stack and load
		std::filesystem::path path = m_ForwardStack.top();
		m_ForwardStack.pop();
		
		// Load without pushing to forward stack
		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		
		if (serializer.Deserialize(path.string())) {
			if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
				newScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
			}
			SetActiveScene(newScene, path);
			return true;
		}
		
		return false;
	}

	// ========================================================================
	// Internal
	// ========================================================================

	void SceneManager::SetActiveScene(Ref<Scene> scene, const std::filesystem::path& path) {
		m_ActiveScene = scene;
		m_ActiveScenePath = path;
		
		// Update main scene entry
		bool found = false;
		for (auto& entry : m_LoadedScenes) {
			if (!entry.IsAdditive) {
				entry.SceneInstance = scene;
				entry.FilePath = path;
				found = true;
				break;
			}
		}
		
		if (!found) {
			SceneEntry entry;
			entry.SceneInstance = scene;
			entry.FilePath = path;
			entry.SceneID = UUID();
			entry.IsAdditive = false;
			entry.IsActive = true;
			m_LoadedScenes.insert(m_LoadedScenes.begin(), entry);
		}
		
		NotifySceneChanged();
	}

	void SceneManager::NotifySceneChanged() {
		if (m_OnSceneChanged) {
			m_OnSceneChanged(m_ActiveScene);
		}
	}

	void SceneManager::NotifyModeChanged(SceneMode oldMode, SceneMode newMode) {
		if (m_OnModeChanged) {
			m_OnModeChanged(oldMode, newMode);
		}
	}

	Ref<Scene> SceneManager::CopySceneForRuntime(Ref<Scene> source) {
		return Scene::Copy(source);
	}

} // namespace Lunex
