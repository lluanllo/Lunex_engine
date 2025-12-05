#pragma once

#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Renderer/EditorCamera.h"

#include "entt.hpp"

#include "../Box2D/include/box2d/box2d.h"

#include <unordered_map>
#include <memory>

namespace Lunex {
	class Entity;
	class ScriptingEngine; // Forward declaration
	
	class Scene {
		public:
			Scene();
			~Scene();
			
			static Ref<Scene> Copy(Ref<Scene> other);
			
			Entity CreateEntity(const std::string& name = std::string());
			Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
			void DestroyEntity(Entity entity);
			
			void OnRuntimeStart();
			void OnRuntimeStop();
			
			void OnSimulationStart();
			void OnSimulationStop();
			
			void OnUpdateRuntime(Timestep ts);
			void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
			void OnUpdateEditor(Timestep ts, EditorCamera& camera);
			void OnViewportResize(uint32_t width, uint32_t height);
			
			void DuplicateEntity(Entity entity);
			
			Entity GetPrimaryCameraEntity();
			
			// ========================================
			// ENTITY HIERARCHY (Parent-Child)
			// ========================================
			void SetParent(Entity child, Entity parent);
			void RemoveParent(Entity child);
			Entity GetParent(Entity entity);
			std::vector<Entity> GetChildren(Entity entity);
			bool IsAncestorOf(Entity potentialAncestor, Entity entity);
			Entity GetEntityByUUID(UUID uuid);
			glm::mat4 GetWorldTransform(Entity entity);
			
			template<typename... Components>
			auto GetAllEntitiesWith() {
				return m_Registry.view<Components...>();
			}
			
		private:
			template<typename T>
			void OnComponentAdded(Entity entity, T& component);
			
			void OnPhysics2DStart();
			void OnPhysics2DStop();
			
			void OnPhysics3DStart();
			void OnPhysics3DStop();
			
			void RenderScene(EditorCamera& camera);
			
			// Helper for recursive duplication
			void DuplicateEntityWithParent(Entity entity, Entity newParent);
			
		private:
			entt::registry m_Registry;
			uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
			
			b2WorldId m_PhysicsWorld = B2_NULL_ID;
			
			// Scripting engine
			std::unique_ptr<ScriptingEngine> m_ScriptingEngine;
			
			friend class Entity;
			friend class SceneSerializer;
			friend class SceneHierarchyPanel;
			friend class ScriptingEngine; // Para acceder al registry
	};
}