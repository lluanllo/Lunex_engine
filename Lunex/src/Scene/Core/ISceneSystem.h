#pragma once

/**
 * @file ISceneSystem.h
 * @brief Base interface for all scene systems
 * 
 * AAA Architecture: All scene systems (Physics, Rendering, Scripting, etc.)
 * implement this interface for unified lifecycle management.
 * 
 * Systems are updated in order of priority and can be enabled/disabled
 * independently for different scene modes (Edit, Play, Simulate).
 */

#include "Core/Timestep.h"
#include "SceneMode.h"
#include <string>
#include <cstdint>

namespace Lunex {

	// Forward declarations
	class Scene;
	class SceneContext;
	struct SceneSystemEvent;

	/**
	 * @enum SceneSystemPriority
	 * @brief Defines update order for systems
	 */
	enum class SceneSystemPriority : uint32_t {
		Input       = 0,      // Input processing first
		Script      = 100,    // Scripts run early
		Physics     = 200,    // Physics simulation
		Animation   = 300,    // Animation updates
		Transform   = 400,    // Transform hierarchy
		Audio       = 500,    // Audio processing
		Render      = 1000,   // Rendering last
		Debug       = 2000    // Debug overlays
	};

	/**
	 * @class ISceneSystem
	 * @brief Abstract base interface for scene systems
	 * 
	 * Scene systems are modular components that handle specific
	 * aspects of scene functionality (physics, rendering, scripting, etc.)
	 */
	class ISceneSystem {
	public:
		virtual ~ISceneSystem() = default;

		// ========== LIFECYCLE ==========
		
		/**
		 * @brief Called when system is added to a scene
		 * @param context Shared context with registry and resources
		 */
		virtual void OnAttach(SceneContext& context) = 0;
		
		/**
		 * @brief Called when system is removed from scene
		 */
		virtual void OnDetach() = 0;
		
		// ========== RUNTIME LIFECYCLE ==========
		
		/**
		 * @brief Called when scene enters Play or Simulate mode
		 * @param mode The mode being started
		 */
		virtual void OnRuntimeStart(SceneMode mode) {}
		
		/**
		 * @brief Called when scene exits Play or Simulate mode
		 */
		virtual void OnRuntimeStop() {}
		
		// ========== UPDATE ==========
		
		/**
		 * @brief Fixed timestep update (for physics, etc.)
		 * @param fixedDeltaTime Fixed delta time
		 */
		virtual void OnFixedUpdate(float fixedDeltaTime) {}
		
		/**
		 * @brief Variable timestep update
		 * @param ts Timestep
		 * @param mode Current scene mode
		 */
		virtual void OnUpdate(Timestep ts, SceneMode mode) = 0;
		
		/**
		 * @brief Late update, called after all regular updates
		 * @param ts Timestep
		 */
		virtual void OnLateUpdate(Timestep ts) {}
		
		// ========== EVENTS ==========
		
		/**
		 * @brief Handle scene events (entity created, destroyed, etc.)
		 * @param event The scene event
		 */
		virtual void OnSceneEvent(const SceneSystemEvent& event) {}
		
		// ========== CONFIGURATION ==========
		
		/**
		 * @brief Get system name for debugging
		 */
		virtual const std::string& GetName() const = 0;
		
		/**
		 * @brief Get system priority for update ordering
		 */
		virtual SceneSystemPriority GetPriority() const = 0;
		
		/**
		 * @brief Check if system should run in given mode
		 * @param mode Scene mode to check
		 */
		virtual bool IsActiveInMode(SceneMode mode) const {
			// By default, systems run in all modes
			return true;
		}
		
		/**
		 * @brief Enable or disable the system
		 */
		virtual void SetEnabled(bool enabled) { m_Enabled = enabled; }
		virtual bool IsEnabled() const { return m_Enabled; }
		
	protected:
		bool m_Enabled = true;
		SceneContext* m_Context = nullptr;
	};

} // namespace Lunex
