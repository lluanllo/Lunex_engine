#pragma once

/**
 * @file ScriptableEntity.h
 * @brief AAA Architecture: Base class for native C++ scripts
 *
 * ScriptableEntity provides the interface for native scripts
 * attached via NativeScriptComponent. This is for scripts compiled
 * directly into the engine, not dynamically loaded DLLs.
 *
 * For dynamic scripting, see Lunex-ScriptCore's Script class.
 */

#include "Entity.h"
#include "Core/Timestep.h"

namespace Lunex {

	// Forward declarations
	class ScriptSystem;
	class Scene;

	/**
	 * @class ScriptableEntity
	 * @brief Base class for native scripts
	 *
	 * Override virtual methods to implement script behavior.
	 * Use GetComponent<T>() to access entity components.
	 */
	class ScriptableEntity {
	public:
		virtual ~ScriptableEntity() = default;

		// ========== COMPONENT ACCESS =========

		/**
		 * @brief Get a component from this entity
		 */
		template<typename T>
		T& GetComponent() {
			return m_Entity.GetComponent<T>();
		}

		/**
		 * @brief Get a component from this entity (const)
		 */
		template<typename T>
		const T& GetComponent() const {
			return m_Entity.GetComponent<T>();
		}

		/**
		 * @brief Check if entity has a component
		 */
		template<typename T>
		bool HasComponent() const {
			return m_Entity.HasComponent<T>();
		}

		/**
		 * @brief Get the entity handle
		 */
		Entity GetEntity() const { return m_Entity; }

		/**
		 * @brief Get the scene this entity belongs to
		 */
		Scene* GetScene() const { return m_Scene; }

	protected:
		// ========== LIFECYCLE HOOKS =========

		/**
		 * @brief Called when script is first created
		 */
		virtual void OnCreate() {}

		/**
		 * @brief Called when script is destroyed
		 */
		virtual void OnDestroy() {}

		/**
		 * @brief Called when script is enabled
		 */
		virtual void OnEnable() {}

		/**
		 * @brief Called when script is disabled
		 */
		virtual void OnDisable() {}

		// ========== UPDATE HOOKS =========

		/**
		 * @brief Called every frame
		 * @param ts Frame timestep
		 */
		virtual void OnUpdate(Timestep ts) {}

		/**
		 * @brief Called at fixed intervals (for physics)
		 * @param fixedDeltaTime Fixed delta time
		 */
		virtual void OnFixedUpdate(float fixedDeltaTime) {}

		/**
		 * @brief Called after all OnUpdate calls
		 * @param ts Frame timestep
		 */
		virtual void OnLateUpdate(Timestep ts) {}

		// ========== PHYSICS HOOKS =========

		/**
		 * @brief Called when a collision starts
		 * @param other The other entity in the collision
		 */
		virtual void OnCollisionEnter(Entity other) {}

		/**
		 * @brief Called every frame while collision is active
		 * @param other The other entity in the collision
		 */
		virtual void OnCollisionStay(Entity other) {}

		/**
		 * @brief Called when a collision ends
		 * @param other The other entity in the collision
		 */
		virtual void OnCollisionExit(Entity other) {}

		/**
		 * @brief Called when entering a trigger
		 * @param other The trigger entity
		 */
		virtual void OnTriggerEnter(Entity other) {}

		/**
		 * @brief Called every frame while in a trigger
		 * @param other The trigger entity
		 */
		virtual void OnTriggerStay(Entity other) {}

		/**
		 * @brief Called when exiting a trigger
		 * @param other The trigger entity
		 */
		virtual void OnTriggerExit(Entity other) {}

	private:
		Entity m_Entity;
		Scene* m_Scene = nullptr;

		friend class Scene;
		friend class ScriptSystem;
	};

} // namespace Lunex