#pragma once

/**
 * @file SceneContext.h
 * @brief Shared context passed to all scene systems
 * 
 * AAA Architecture: SceneContext provides systems with access to
 * shared resources without coupling them to the Scene class directly.
 */

#include "Core/Core.h"
#include "Core/UUID.h"
#include "SceneMode.h"
#include "SceneEvents.h"
#include "entt.hpp"

#include <glm/glm.hpp>
#include <functional>
#include <vector>

namespace Lunex {

	// Forward declarations
	class Scene;
	class Entity;
	class EditorCamera;

	/**
	 * @struct SceneContext
	 * @brief Shared context for scene systems
	 * 
	 * Provides read/write access to scene data without exposing
	 * the full Scene API to individual systems.
	 */
	struct SceneContext {
		// ========== CORE REFERENCES ==========
		
		// The ECS registry (owned by Scene)
		entt::registry* Registry = nullptr;
		
		// Pointer back to owning scene (for complex operations)
		Scene* OwningScene = nullptr;
		
		// ========== SCENE STATE ==========
		
		// Current scene mode
		SceneMode Mode = SceneMode::Edit;
		
		// Viewport dimensions
		uint32_t ViewportWidth = 0;
		uint32_t ViewportHeight = 0;
		
		// ========== TIMING ==========
		
		// Fixed timestep for physics (default 60 FPS)
		float FixedTimestep = 1.0f / 60.0f;
		
		// Accumulated time for fixed update
		float FixedTimeAccumulator = 0.0f;
		
		// Max substeps per frame (prevent spiral of death)
		int MaxSubsteps = 8;
		
		// ========== EVENT SYSTEM ==========
		
		using EventCallback = std::function<void(const SceneSystemEvent&)>;
		std::vector<EventCallback> EventListeners;
		
		/**
		 * @brief Dispatch an event to all listeners
		 */
		void DispatchEvent(const SceneSystemEvent& event) {
			for (auto& listener : EventListeners) {
				listener(event);
			}
		}
		
		/**
		 * @brief Register an event listener
		 */
		void AddEventListener(EventCallback callback) {
			EventListeners.push_back(std::move(callback));
		}
		
		// ========== HELPER METHODS ==========
		
		/**
		 * @brief Check if we're in a runtime mode
		 */
		bool IsRuntime() const {
			return IsRuntimeMode(Mode);
		}
		
		/**
		 * @brief Check if physics should be active
		 */
		bool IsPhysicsActive() const {
			return IsPhysicsActiveInMode(Mode);
		}
		
		/**
		 * @brief Check if scripts should be active
		 */
		bool IsScriptActive() const {
			return IsScriptActiveInMode(Mode);
		}
		
		/**
		 * @brief Get aspect ratio
		 */
		float GetAspectRatio() const {
			if (ViewportHeight == 0) return 1.0f;
			return static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
		}
	};

	/**
	 * @struct SystemUpdateContext
	 * @brief Per-frame update context passed to systems
	 */
	struct SystemUpdateContext {
		// Reference to shared context
		SceneContext* Context = nullptr;
		
		// Delta time for this frame
		float DeltaTime = 0.0f;
		
		// Total elapsed time
		float TotalTime = 0.0f;
		
		// Frame number
		uint64_t FrameNumber = 0;
		
		// Editor camera (nullptr during Play mode)
		EditorCamera* EditorCam = nullptr;
	};

} // namespace Lunex
