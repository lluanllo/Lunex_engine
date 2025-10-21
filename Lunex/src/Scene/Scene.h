#pragma once

#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Renderer/EditorCamera.h"

#include "entt.hpp"

#include "../Box2D/include/box2d/box2d.h"

namespace Lunex {
	class Entity;
	
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
			
			template<typename... Components>
			auto GetAllEntitiesWith() {
				return m_Registry.view<Components...>();
			}
			
		private:
			template<typename T>
			void OnComponentAdded(Entity entity, T& component);
			
			void OnPhysics2DStart();
			void OnPhysics2DStop();
			
			void RenderScene(EditorCamera& camera);
		private:
			entt::registry m_Registry;
			uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
			
			b2WorldId m_PhysicsWorld = B2_NULL_ID;
			
			friend class Entity;
			friend class SceneSerializer;
			friend class SceneHierarchyPanel;
	};
}