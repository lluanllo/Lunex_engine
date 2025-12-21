#pragma once

/**
 * @file SceneManager.h
 * @brief Multi-scene management for AAA architecture
 * 
 * AAA Architecture: SceneManager handles scene lifecycle,
 * transitions, and subscene loading (additive scenes).
 */

#include "Core/Core.h"
#include "Core/UUID.h"
#include "Core/Timestep.h"  // Added for Timestep
#include "Scene/Core/SceneMode.h"

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>
#include <stack>

namespace Lunex {

	// Forward declarations
	class Scene;
	class EditorCamera;

	/**
	 * @enum SceneTransitionType
	 * @brief Types of scene transitions
	 */
	enum class SceneTransitionType : uint8_t {
		Instant = 0,       // Immediate switch
		Fade = 1,          // Fade out/in
		CrossFade = 2,     // Overlay fade
		Custom = 3         // Custom transition
	};

	/**
	 * @struct SceneTransition
	 * @brief Configuration for scene transition
	 */
	struct SceneTransition {
		SceneTransitionType Type = SceneTransitionType::Instant;
		float Duration = 0.5f;
		std::function<void()> OnTransitionStart;
		std::function<void()> OnTransitionComplete;
	};

	/**
	 * @struct SceneEntry
	 * @brief Entry in the scene stack/registry
	 */
	struct SceneEntry {
		Ref<Scene> SceneInstance;
		std::filesystem::path FilePath;
		UUID SceneID = 0;
		bool IsAdditive = false;     // Loaded on top of main scene
		bool IsPersistent = false;   // Don't unload on scene change
		bool IsActive = true;        // Currently updating
	};

	/**
	 * @class SceneManager
	 * @brief Manages scene lifecycle and transitions
	 * 
	 * Features:
	 * - Single active scene with optional additive scenes
	 * - Scene transitions with effects
	 * - Scene stack for navigation (back/forward)
	 * - Persistent scenes (don't unload on change)
	 */
	class SceneManager {
	public:
		SceneManager();
		~SceneManager();
		
		// ========== LIFECYCLE ==========
		
		/**
		 * @brief Initialize the scene manager
		 */
		void Initialize();
		
		/**
		 * @brief Shutdown and cleanup
		 */
		void Shutdown();
		
		// ========== SCENE LOADING ==========
		
		/**
		 * @brief Create a new empty scene
		 * @return The new scene
		 */
		Ref<Scene> CreateScene(const std::string& name = "New Scene");
		
		/**
		 * @brief Load scene from file
		 * @param path Path to scene file
		 * @param transition Optional transition effect
		 * @return Success
		 */
		bool LoadScene(const std::filesystem::path& path, 
		              const SceneTransition& transition = {});
		
		/**
		 * @brief Load scene additively (on top of current)
		 * @param path Path to scene file
		 * @return Success
		 */
		bool LoadSceneAdditive(const std::filesystem::path& path);
		
		/**
		 * @brief Unload an additive scene
		 * @param sceneID UUID of scene to unload
		 */
		void UnloadAdditiveScene(UUID sceneID);
		
		/**
		 * @brief Unload all additive scenes
		 */
		void UnloadAllAdditiveScenes();
		
		// ========== SCENE ACCESS ==========
		
		/**
		 * @brief Get the main active scene
		 */
		Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
		
		/**
		 * @brief Get all loaded scenes (main + additive)
		 */
		const std::vector<SceneEntry>& GetLoadedScenes() const { return m_LoadedScenes; }
		
		/**
		 * @brief Get scene by UUID
		 */
		Ref<Scene> GetScene(UUID sceneID) const;
		
		// ========== SCENE MODE ==========
		
		/**
		 * @brief Get current scene mode
		 */
		SceneMode GetMode() const { return m_Mode; }
		
		/**
		 * @brief Start play mode
		 */
		void StartPlay();
		
		/**
		 * @brief Start simulate mode (physics only)
		 */
		void StartSimulate();
		
		/**
		 * @brief Stop runtime and return to edit mode
		 */
		void Stop();
		
		/**
		 * @brief Pause runtime
		 */
		void Pause();
		
		/**
		 * @brief Resume from pause
		 */
		void Resume();
		
		// ========== UPDATE ==========
		
		/**
		 * @brief Update all active scenes
		 * @param ts Timestep
		 * @param editorCamera Editor camera (nullptr in Play mode)
		 */
		void OnUpdate(Timestep ts, EditorCamera* editorCamera = nullptr);
		
		/**
		 * @brief Handle viewport resize
		 */
		void OnViewportResize(uint32_t width, uint32_t height);
		
		// ========== NAVIGATION ==========
		
		/**
		 * @brief Go back to previous scene
		 */
		bool NavigateBack();
		
		/**
		 * @brief Go forward (if navigated back)
		 */
		bool NavigateForward();
		
		/**
		 * @brief Check if can navigate back
		 */
		bool CanNavigateBack() const { return !m_BackStack.empty(); }
		
		/**
		 * @brief Check if can navigate forward
		 */
		bool CanNavigateForward() const { return !m_ForwardStack.empty(); }
		
		// ========== CALLBACKS ==========
		
		using SceneChangeCallback = std::function<void(Ref<Scene>)>;
		using ModeChangeCallback = std::function<void(SceneMode, SceneMode)>;
		
		void SetOnSceneChanged(SceneChangeCallback callback) { m_OnSceneChanged = std::move(callback); }
		void SetOnModeChanged(ModeChangeCallback callback) { m_OnModeChanged = std::move(callback); }
		
		// ========== SINGLETON ==========
		
		static SceneManager& Get() { return *s_Instance; }
		
	private:
		void SetActiveScene(Ref<Scene> scene, const std::filesystem::path& path = {});
		void NotifySceneChanged();
		void NotifyModeChanged(SceneMode oldMode, SceneMode newMode);
		
		Ref<Scene> CopySceneForRuntime(Ref<Scene> source);
		
	private:
		static SceneManager* s_Instance;
		
		// Main scene
		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;  // Copy of scene for edit mode (restored on stop)
		std::filesystem::path m_ActiveScenePath;
		
		// Scene mode
		SceneMode m_Mode = SceneMode::Edit;
		
		// Loaded scenes (main + additive)
		std::vector<SceneEntry> m_LoadedScenes;
		
		// Navigation stacks
		std::stack<std::filesystem::path> m_BackStack;
		std::stack<std::filesystem::path> m_ForwardStack;
		
		// Viewport
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		
		// Callbacks
		SceneChangeCallback m_OnSceneChanged;
		ModeChangeCallback m_OnModeChanged;
		
		// Transition state
		bool m_IsTransitioning = false;
		SceneTransition m_CurrentTransition;
		float m_TransitionProgress = 0.0f;
	};

} // namespace Lunex
