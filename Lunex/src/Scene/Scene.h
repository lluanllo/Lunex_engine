#pragma once

/**
 * @file Scene.h
 * @brief Main scene class with system orchestration
 * 
 * AAA Architecture: Scene is the container for entities and systems.
 * Systems (Physics, Rendering, Scripting) are modular and pluggable.
 */

#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Scene/Core/SceneContext.h"
#include "Scene/Core/SceneMode.h"
#include "Scene/Core/SceneEvents.h"
#include "Scene/Core/ISceneSystem.h"
#include "Scene/Camera/EditorCamera.h"

#include "entt.hpp"

#include "../Box2D/include/box2d/box2d.h"

#include <unordered_map>
#include <memory>
#include <vector>

namespace Lunex {
	class Entity;
	class ScriptingEngine;
	
	// Forward declare systems
	class PhysicsSystem2D;
	class PhysicsSystem3D;
	class ScriptSystem;
	class SceneRenderSystem;
	
	class Scene {
		public:
			Scene();
			~Scene();
			
			static Ref<Scene> Copy(Ref<Scene> other);
			
			// ========== ENTITY MANAGEMENT ==========
			Entity CreateEntity(const std::string& name = std::string());
			Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
			void DestroyEntity(Entity entity);
			void DuplicateEntity(Entity entity);
			
			// ========== RUNTIME LIFECYCLE ==========
			void OnRuntimeStart();
			void OnRuntimeStop();
			
			void OnSimulationStart();
			void OnSimulationStop();
			
			// ========== UPDATE ==========
			void OnUpdateRuntime(Timestep ts);
			void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
			void OnUpdateEditor(Timestep ts, EditorCamera& camera);
			void OnViewportResize(uint32_t width, uint32_t height);
			
			// ========== ENTITY HIERARCHY ==========
			void SetParent(Entity child, Entity parent);
			void RemoveParent(Entity child);
			Entity GetParent(Entity entity);
			std::vector<Entity> GetChildren(Entity entity);
			bool IsAncestorOf(Entity potentialAncestor, Entity entity);
			Entity GetEntityByUUID(UUID uuid);
			glm::mat4 GetWorldTransform(Entity entity);
			
			Entity GetPrimaryCameraEntity();
			
			// ========== SYSTEM ACCESS (AAA Architecture) ==========
			
			/**
			 * @brief Get a system by type
			 */
			template<typename T>
			T* GetSystem() {
				for (auto& system : m_Systems) {
					if (T* typed = dynamic_cast<T*>(system.get())) {
						return typed;
					}
				}
				return nullptr;
			}
			
			/**
			 * @brief Get the scene context
			 */
			SceneContext& GetContext() { return m_Context; }
			const SceneContext& GetContext() const { return m_Context; }
			
			/**
			 * @brief Get current scene mode
			 */
			SceneMode GetMode() const { return m_Context.Mode; }
			
			/**
			 * @brief Dispatch a scene event to all systems
			 */
			void DispatchEvent(const SceneSystemEvent& event);
			
			// ========== ENTITY QUERIES ==========
			template<typename... Components>
			auto GetAllEntitiesWith() {
				return m_Registry.view<Components...>();
			}
			
		private:
			template<typename T>
			void OnComponentAdded(Entity entity, T& component);
			
			// ========== LEGACY PHYSICS (to be migrated) ==========
			void OnPhysics2DStart();
			void OnPhysics2DStop();
			
			void OnPhysics3DStart();
			void OnPhysics3DStop();
			
			void RenderScene(EditorCamera& camera);
			
			void DuplicateEntityWithParent(Entity entity, Entity newParent);
			
			// ========== SYSTEM MANAGEMENT ==========
			void InitializeSystems();
			void ShutdownSystems();
			void UpdateSystems(Timestep ts, SceneMode mode);
			
		private:
			entt::registry m_Registry;
			SceneContext m_Context;
			
			// Scene systems (ordered by priority)
			std::vector<std::unique_ptr<ISceneSystem>> m_Systems;
			
			friend class Entity;
			friend class SceneSerializer;
			friend class SceneHierarchyPanel;
			friend class ScriptingEngine;
			friend class PhysicsSystem;
			friend class PhysicsSystem2D;
			friend class PhysicsSystem3D;
			friend class ScriptSystem;
			friend class SceneRenderSystem;
	};
}
