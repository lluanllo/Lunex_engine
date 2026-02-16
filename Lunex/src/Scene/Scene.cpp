#include "stpch.h"
#include "Scene.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/SkyboxRenderer.h"
#include "RHI/RHI.h"
#include "Core/Input.h"
#include "Core/JobSystem/JobSystem.h"

#include "Scripting/ScriptingEngine.h"
#include "Physics/Physics.h"

// AAA Architecture: Include Systems
#include "Scene/Systems/PhysicsSystem2D.h"
#include "Scene/Systems/PhysicsSystem3D.h"
#include "Scene/Systems/ScriptSystem.h"
#include "Scene/Systems/SceneRenderSystem.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "Entity.h"

namespace Lunex {

	Scene::Scene() {
		// Initialize scene context
		m_Context.Registry = &m_Registry;
		m_Context.OwningScene = this;
		m_Context.Mode = SceneMode::Edit;
		
		// Initialize AAA systems
		InitializeSystems();
	}

	Scene::~Scene() {
		// Shutdown all systems (they handle their own cleanup)
		ShutdownSystems();
	}
	
	// ========================================================================
	// SYSTEM MANAGEMENT (AAA Architecture)
	// ========================================================================
	
	void Scene::InitializeSystems() {
		// Create and attach systems in priority order
		// Note: Systems are stored sorted by priority
		
		// AAA Architecture: All systems are now fully integrated
		
		// Script System (priority: 100)
		auto scriptSystem = std::make_unique<ScriptSystem>();
		scriptSystem->OnAttach(m_Context);
		m_Systems.push_back(std::move(scriptSystem));
		
		// Physics 2D System (priority: 200)
		auto physics2D = std::make_unique<PhysicsSystem2D>();
		physics2D->OnAttach(m_Context);
		m_Systems.push_back(std::move(physics2D));
		
		// Physics 3D System (priority: 200)
		auto physics3D = std::make_unique<PhysicsSystem3D>();
		physics3D->OnAttach(m_Context);
		m_Systems.push_back(std::move(physics3D));
		
		// Render System (priority: 1000)
		auto renderSystem = std::make_unique<SceneRenderSystem>();
		renderSystem->OnAttach(m_Context);
		m_Systems.push_back(std::move(renderSystem));
		
		// Sort systems by priority
		std::sort(m_Systems.begin(), m_Systems.end(),
			[](const std::unique_ptr<ISceneSystem>& a, const std::unique_ptr<ISceneSystem>& b) {
				return static_cast<uint32_t>(a->GetPriority()) < static_cast<uint32_t>(b->GetPriority());
			});
		
		LNX_LOG_INFO("Scene systems initialized ({0} systems)", m_Systems.size());
	}
	
	void Scene::ShutdownSystems() {
		// Shutdown in reverse order
		for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it) {
			(*it)->OnDetach();
		}
		m_Systems.clear();
	}
	
	void Scene::UpdateSystems(Timestep ts, SceneMode mode) {
		for (auto& system : m_Systems) {
			if (system->IsEnabled() && system->IsActiveInMode(mode)) {
				system->OnUpdate(ts, mode);
			}
		}
	}
	
	void Scene::DispatchEvent(const SceneSystemEvent& event) {
		// Dispatch to context listeners
		m_Context.DispatchEvent(event);
		
		// Dispatch to each system
		for (auto& system : m_Systems) {
			system->OnSceneEvent(event);
		}
	}

	// ...existing code for CopyComponent templates...

	template<typename... Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap) {
		([&]()
			{
				auto view = src.view<Component>();
				for (auto srcEntity : view) {
					entt::entity dstEntity = enttMap.at(src.get<IDComponent>(srcEntity).ID);

					auto& srcComponent = src.get<Component>(srcEntity);
					dst.emplace_or_replace<Component>(dstEntity, srcComponent);
				}
			}(), ...);
	}

	template<typename... Component>
	static void CopyComponent(ComponentGroup<Component...>, entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap) {
		CopyComponent<Component...>(dst, src, enttMap);
	}

	template<typename... Component>
	static void CopyComponentIfExists(Entity dst, Entity src) {
		([&]()
			{
				if (src.HasComponent<Component>())
				dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
			}(), ...);
	}

	template<typename... Component>
	static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src) {
		CopyComponentIfExists<Component...>(dst, src);
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other) {
		Ref<Scene> newScene = CreateRef<Scene>();

		newScene->m_Context.ViewportWidth = other->m_Context.ViewportWidth;
		newScene->m_Context.ViewportHeight = other->m_Context.ViewportHeight;

		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene
		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto e : idView) {
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, enttMap);

		return newScene;
	}

	Entity Scene::CreateEntity(const std::string& name) {
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		
		// Dispatch entity created event
		DispatchEvent(SceneSystemEvent::EntityCreated(entity, uuid));

		return entity;
	}

	void Scene::DestroyEntity(Entity entity) {
		// Dispatch event before destruction
		UUID uuid = entity.GetUUID();
		DispatchEvent(SceneSystemEvent::EntityDestroyed(entity, uuid));
		
		m_Registry.destroy(entity);
	}

	void Scene::OnRuntimeStart() {
		m_Context.Mode = SceneMode::Play;
		
		// Start all AAA systems (Script, Physics2D, Physics3D, Render)
		for (auto& system : m_Systems) {
			if (system->IsEnabled()) {
				system->OnRuntimeStart(SceneMode::Play);
			}
		}
	}

	void Scene::OnRuntimeStop() {
		// Stop all AAA systems
		for (auto& system : m_Systems) {
			system->OnRuntimeStop();
		}
		
		m_Context.Mode = SceneMode::Edit;
	}

	void Scene::OnSimulationStart() {
		m_Context.Mode = SceneMode::Simulate;
		
		// Start all AAA systems
		for (auto& system : m_Systems) {
			if (system->IsEnabled()) {
				system->OnRuntimeStart(SceneMode::Simulate);
			}
		}
	}

	void Scene::OnSimulationStop() {
		// Stop all AAA systems
		for (auto& system : m_Systems) {
			system->OnRuntimeStop();
		}
		
		m_Context.Mode = SceneMode::Edit;
	}

	void Scene::OnUpdateRuntime(Timestep ts) {
		// Update all AAA systems (Script, Physics2D, Physics3D)
		UpdateSystems(ts, SceneMode::Play);

		// Get render system and render with runtime camera
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view) {
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary) {
					mainCamera = &camera.Camera;
					cameraTransform = transform.GetTransform();
					break;
				}
			}
		}

		if (mainCamera) {
			// Use SceneRenderSystem for runtime rendering
			if (auto* renderSystem = GetSystem<SceneRenderSystem>()) {
				renderSystem->RenderSceneRuntime(*mainCamera, cameraTransform);
			}
		}
	}

	void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& camera) {
		// Update all AAA systems (Physics2D, Physics3D - Scripts not active in Simulate)
		UpdateSystems(ts, SceneMode::Simulate);
		
		// Use SceneRenderSystem for editor rendering
		if (auto* renderSystem = GetSystem<SceneRenderSystem>()) {
			renderSystem->RenderScene(camera);
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
		// Update AAA systems (only those active in Edit mode)
		UpdateSystems(ts, SceneMode::Edit);
		
		// Use SceneRenderSystem for editor rendering
		if (auto* renderSystem = GetSystem<SceneRenderSystem>()) {
			renderSystem->RenderScene(camera);
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height) {
		m_Context.ViewportWidth = width;
		m_Context.ViewportHeight = height;
		
		// Dispatch viewport resize event
		DispatchEvent(SceneSystemEvent::ViewportResized(width, height));

		// Resize our non-FixedAspectRatio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view) {
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	Entity Scene::GetPrimaryCameraEntity() {
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view) {
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{ entity, this };
		}
		return {};
	}

	// ========================================
	// LEGACY PHYSICS FUNCTIONS (DEPRECATED - Now handled by PhysicsSystem2D/3D)
	// These are kept for backwards compatibility but are no longer called
	// ========================================

	void Scene::OnPhysics2DStart() {
		// DEPRECATED: Physics initialization is now handled by PhysicsSystem2D
		LNX_LOG_WARN("Scene::OnPhysics2DStart is deprecated - use PhysicsSystem2D");
	}

	void Scene::OnPhysics2DStop() {
		// DEPRECATED: Physics cleanup is now handled by PhysicsSystem2D
		LNX_LOG_WARN("Scene::OnPhysics2DStop is deprecated - use PhysicsSystem2D");
	}
	
	void Scene::OnPhysics3DStart() {
		// DEPRECATED: Physics initialization is now handled by PhysicsSystem3D
		LNX_LOG_WARN("Scene::OnPhysics3DStart is deprecated - use PhysicsSystem3D");
	}

	void Scene::OnPhysics3DStop() {
		// DEPRECATED: Physics cleanup is now handled by PhysicsSystem3D
		LNX_LOG_WARN("Scene::OnPhysics3DStop is deprecated - use PhysicsSystem3D");
	}

	void Scene::RenderScene(EditorCamera& camera) {
		// DEPRECATED: Rendering is now handled by SceneRenderSystem
		// This function is kept for backwards compatibility
		if (auto* renderSystem = GetSystem<SceneRenderSystem>()) {
			renderSystem->RenderScene(camera);
		}
	}

	void Scene::DuplicateEntity(Entity entity) {
		if (!entity)
			return;
			
		Entity newEntity = CreateEntity(entity.GetName());
		CopyComponentIfExists(AllComponents{}, newEntity, entity);
		
		// Clear relationship on duplicated entity
		if (newEntity.HasComponent<RelationshipComponent>()) {
			newEntity.GetComponent<RelationshipComponent>() = RelationshipComponent{};
		}
		
		// Recursively duplicate children
		if (entity.HasComponent<RelationshipComponent>()) {
			auto& rel = entity.GetComponent<RelationshipComponent>();
			for (UUID childUUID : rel.ChildrenIDs) {
				Entity child = GetEntityByUUID(childUUID);
				if (child) {
					DuplicateEntityWithParent(child, newEntity);
				}
			}
		}
	}
	
	void Scene::DuplicateEntityWithParent(Entity entity, Entity newParent) {
		if (!entity)
			return;
			
		Entity newEntity = CreateEntity(entity.GetName());
		CopyComponentIfExists(AllComponents{}, newEntity, entity);
		
		// Clear and set new parent
		if (newEntity.HasComponent<RelationshipComponent>()) {
			newEntity.GetComponent<RelationshipComponent>() = RelationshipComponent{};
		}
		SetParent(newEntity, newParent);
		
		// Recursively duplicate children
		if (entity.HasComponent<RelationshipComponent>()) {
			auto& rel = entity.GetComponent<RelationshipComponent>();
			for (UUID childUUID : rel.ChildrenIDs) {
				Entity child = GetEntityByUUID(childUUID);
				if (child) {
					DuplicateEntityWithParent(child, newEntity);
				}
			}
		}
	}

	// ========================================
	// COMPONENT ADDED CALLBACKS
	// ========================================

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) {
		static_assert(sizeof(T) == 0);
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component) {
		if (m_Context.ViewportWidth > 0 && m_Context.ViewportHeight > 0)
			component.Camera.SetViewportSize(m_Context.ViewportWidth, m_Context.ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<MaterialComponent>(Entity entity, MaterialComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<TextureComponent>(Entity entity, TextureComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<Rigidbody3DComponent>(Entity entity, Rigidbody3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<BoxCollider3DComponent>(Entity entity, BoxCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<SphereCollider3DComponent>(Entity entity, SphereCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CapsuleCollider3DComponent>(Entity entity, CapsuleCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<MeshCollider3DComponent>(Entity entity, MeshCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CylinderCollider3DComponent>(Entity entity, CylinderCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<ConeCollider3DComponent>(Entity entity, ConeCollider3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<CharacterController3DComponent>(Entity entity, CharacterController3DComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component) {
	}

	template<>
	void Scene::OnComponentAdded<EnvironmentComponent>(Entity entity, EnvironmentComponent& component) {
	}

	// ========================================
	// ENTITY HIERARCHY (Parent-Child)
	// ========================================

	Entity Scene::GetEntityByUUID(UUID uuid) {
		auto view = m_Registry.view<IDComponent>();
		for (auto e : view) {
			if (view.get<IDComponent>(e).ID == uuid) {
				return Entity{ e, this };
			}
		}
		return Entity{};
	}

	void Scene::SetParent(Entity child, Entity parent) {
		if (!child || child == parent)
			return;

		if (parent && IsAncestorOf(child, parent))
			return;

		UUID childUUID = child.GetComponent<IDComponent>().ID;

		// Save child's world position before reparenting
		glm::vec3 childWorldPos = glm::vec3(0.0f);
		if (child.HasComponent<TransformComponent>()) {
			childWorldPos = glm::vec3(GetWorldTransform(child)[3]);
		}

		RemoveParent(child);

		if (!child.HasComponent<RelationshipComponent>()) {
			child.AddComponent<RelationshipComponent>();
		}

		auto& childRel = child.GetComponent<RelationshipComponent>();

		if (parent) {
			if (!parent.HasComponent<RelationshipComponent>()) {
				parent.AddComponent<RelationshipComponent>();
			}

			UUID parentUUID = parent.GetComponent<IDComponent>().ID;
			childRel.SetParent(parentUUID);
			
			auto& parentRel = parent.GetComponent<RelationshipComponent>();
			parentRel.AddChild(childUUID);

			// Adjust local position to maintain world position
			if (child.HasComponent<TransformComponent>() && parent.HasComponent<TransformComponent>()) {
				glm::vec3 parentWorldPos = glm::vec3(GetWorldTransform(parent)[3]);
				child.GetComponent<TransformComponent>().Translation = childWorldPos - parentWorldPos;
			}
			
			// Dispatch hierarchy event
			DispatchEvent(SceneSystemEvent::ParentChanged(child, parent, childUUID, parentUUID));
		}
	}

	void Scene::RemoveParent(Entity child) {
		if (!child || !child.HasComponent<RelationshipComponent>())
			return;

		auto& childRel = child.GetComponent<RelationshipComponent>();
		
		if (!childRel.HasParent())
			return;

		// Save world position before unparenting
		glm::vec3 childWorldPos = glm::vec3(0.0f);
		if (child.HasComponent<TransformComponent>()) {
			childWorldPos = glm::vec3(GetWorldTransform(child)[3]);
		}

		UUID childUUID = child.GetComponent<IDComponent>().ID;
		UUID parentUUID = childRel.ParentID;

		Entity parent = GetEntityByUUID(parentUUID);
		if (parent && parent.HasComponent<RelationshipComponent>()) {
			parent.GetComponent<RelationshipComponent>().RemoveChild(childUUID);
		}

		childRel.ClearParent();

		// Restore world position as local position (no parent now)
		if (child.HasComponent<TransformComponent>()) {
			child.GetComponent<TransformComponent>().Translation = childWorldPos;
		}
	}

	Entity Scene::GetParent(Entity entity) {
		if (!entity || !entity.HasComponent<RelationshipComponent>())
			return Entity{};

		auto& rel = entity.GetComponent<RelationshipComponent>();
		if (!rel.HasParent())
			return Entity{};

		return GetEntityByUUID(rel.ParentID);
	}

	std::vector<Entity> Scene::GetChildren(Entity entity) {
		std::vector<Entity> children;

		if (!entity || !entity.HasComponent<RelationshipComponent>())
			return children;

		auto& rel = entity.GetComponent<RelationshipComponent>();
		for (UUID childUUID : rel.ChildrenIDs) {
			Entity child = GetEntityByUUID(childUUID);
			if (child) {
				children.push_back(child);
			}
		}

		return children;
	}

	bool Scene::IsAncestorOf(Entity potentialAncestor, Entity entity) {
		if (!potentialAncestor || !entity)
			return false;

		Entity current = GetParent(entity);
		while (current) {
			if (current == potentialAncestor)
				return true;
			current = GetParent(current);
		}

		return false;
	}

	glm::mat4 Scene::GetWorldTransform(Entity entity) {
		if (!entity || !entity.HasComponent<TransformComponent>())
			return glm::mat4(1.0f);
		
		auto& transform = entity.GetComponent<TransformComponent>();
		glm::mat4 localTransform = transform.GetTransform();
		
		Entity parent = GetParent(entity);
		if (parent && parent.HasComponent<TransformComponent>()) {
			// Only inherit parent's position, not rotation/scale
			glm::vec3 parentWorldPos = glm::vec3(GetWorldTransform(parent)[3]);
			
			// Apply parent position offset
			glm::mat4 parentOffset = glm::translate(glm::mat4(1.0f), parentWorldPos);
			return parentOffset * localTransform;
		}
		
		return localTransform;
	}

} // namespace Lunex