#pragma once

/**
 * @file SceneEvents.h
 * @brief Scene system events for inter-system communication
 * 
 * AAA Architecture: Scene events allow systems to react to
 * scene changes without tight coupling.
 */

#include "Core/UUID.h"
#include "entt.hpp"
#include <variant>
#include <string>

namespace Lunex {

	// Forward declarations
	class Entity;

	/**
	 * @enum SceneEventType
	 * @brief Types of scene events
	 */
	enum class SceneEventType : uint8_t {
		None = 0,
		
		// Entity lifecycle
		EntityCreated,
		EntityDestroyed,
		EntityEnabled,
		EntityDisabled,
		
		// Component lifecycle
		ComponentAdded,
		ComponentRemoved,
		ComponentModified,
		
		// Hierarchy
		ParentChanged,
		ChildAdded,
		ChildRemoved,
		
		// Scene lifecycle
		SceneLoaded,
		SceneUnloaded,
		SceneModeChanged,
		
		// Viewport
		ViewportResized
	};

	/**
	 * @struct EntityEventData
	 * @brief Data for entity-related events
	 */
	struct EntityEventData {
		entt::entity EntityHandle = entt::null;
		UUID EntityID = 0;
	};

	/**
	 * @struct ComponentEventData
	 * @brief Data for component-related events
	 */
	struct ComponentEventData {
		entt::entity EntityHandle = entt::null;
		UUID EntityID = 0;
		size_t ComponentTypeHash = 0;  // typeid(T).hash_code()
		std::string ComponentTypeName;
	};

	/**
	 * @struct HierarchyEventData
	 * @brief Data for hierarchy-related events
	 */
	struct HierarchyEventData {
		entt::entity ChildHandle = entt::null;
		entt::entity ParentHandle = entt::null;
		UUID ChildID = 0;
		UUID ParentID = 0;
	};

	/**
	 * @struct ViewportEventData
	 * @brief Data for viewport-related events
	 */
	struct ViewportEventData {
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	/**
	 * @struct SceneModeEventData
	 * @brief Data for scene mode change events
	 */
	struct SceneModeEventData {
		uint8_t OldMode = 0;
		uint8_t NewMode = 0;
	};

	/**
	 * @struct SceneSystemEvent
	 * @brief Unified event structure for scene systems
	 */
	struct SceneSystemEvent {
		SceneEventType Type = SceneEventType::None;
		
		// Event data (variant for type-safe storage)
		std::variant<
			std::monostate,
			EntityEventData,
			ComponentEventData,
			HierarchyEventData,
			ViewportEventData,
			SceneModeEventData
		> Data;
		
		// ========== FACTORY METHODS ==========
		
		static SceneSystemEvent EntityCreated(entt::entity handle, UUID id) {
			SceneSystemEvent e;
			e.Type = SceneEventType::EntityCreated;
			e.Data = EntityEventData{ handle, id };
			return e;
		}
		
		static SceneSystemEvent EntityDestroyed(entt::entity handle, UUID id) {
			SceneSystemEvent e;
			e.Type = SceneEventType::EntityDestroyed;
			e.Data = EntityEventData{ handle, id };
			return e;
		}
		
		template<typename T>
		static SceneSystemEvent ComponentAdded(entt::entity handle, UUID id) {
			SceneSystemEvent e;
			e.Type = SceneEventType::ComponentAdded;
			e.Data = ComponentEventData{ 
				handle, 
				id, 
				typeid(T).hash_code(),
				typeid(T).name()
			};
			return e;
		}
		
		template<typename T>
		static SceneSystemEvent ComponentRemoved(entt::entity handle, UUID id) {
			SceneSystemEvent e;
			e.Type = SceneEventType::ComponentRemoved;
			e.Data = ComponentEventData{ 
				handle, 
				id, 
				typeid(T).hash_code(),
				typeid(T).name()
			};
			return e;
		}
		
		static SceneSystemEvent ParentChanged(entt::entity child, entt::entity parent, UUID childID, UUID parentID) {
			SceneSystemEvent e;
			e.Type = SceneEventType::ParentChanged;
			e.Data = HierarchyEventData{ child, parent, childID, parentID };
			return e;
		}
		
		static SceneSystemEvent ViewportResized(uint32_t width, uint32_t height) {
			SceneSystemEvent e;
			e.Type = SceneEventType::ViewportResized;
			e.Data = ViewportEventData{ width, height };
			return e;
		}
		
		static SceneSystemEvent ModeChanged(uint8_t oldMode, uint8_t newMode) {
			SceneSystemEvent e;
			e.Type = SceneEventType::SceneModeChanged;
			e.Data = SceneModeEventData{ oldMode, newMode };
			return e;
		}
		
		// ========== ACCESSORS ==========
		
		template<typename T>
		const T* GetData() const {
			return std::get_if<T>(&Data);
		}
	};

} // namespace Lunex
