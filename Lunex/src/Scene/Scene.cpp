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
	static b2BodyType Rigidbody2DTypeToBox2DBody(Rigidbody2DComponent::BodyType bodyType) {
		switch (bodyType) {
		case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
		case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
		case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
		}

		LNX_CORE_ASSERT(false, "Unknown body type");
		return b2_staticBody;
	}

	Scene::Scene() {
		// Initialize scene context
		m_Context.Registry = &m_Registry;
		m_Context.OwningScene = this;
		m_Context.Mode = SceneMode::Edit;
		
		// Initialize AAA systems (ScriptSystem now handles scripting)
		InitializeSystems();
		
		// Legacy scripting engine is no longer needed - ScriptSystem handles it
		// m_ScriptingEngine = std::make_unique<ScriptingEngine>();
		// m_ScriptingEngine->Initialize(this);
	}

	Scene::~Scene() {
		// Shutdown systems first
		ShutdownSystems();
		
		// Legacy cleanup
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			b2DestroyWorld(m_PhysicsWorld);
		}
	}
	
	// ========================================================================
	// SYSTEM MANAGEMENT (AAA Architecture)
	// ========================================================================
	
	void Scene::InitializeSystems() {
		// Create and attach systems in priority order
		// Note: Systems are stored sorted by priority
		
		// AAA Architecture: Systems are now fully integrated
		// They coexist with legacy code during transition period
		
		// Script System (priority: 100)
		auto scriptSystem = std::make_unique<ScriptSystem>();
		scriptSystem->OnAttach(m_Context);
		m_Systems.push_back(std::move(scriptSystem));
		
		// Physics 2D System (priority: 200) - Legacy code still handles physics for now
		// auto physics2D = std::make_unique<PhysicsSystem2D>();
		// physics2D->OnAttach(m_Context);
		// m_Systems.push_back(std::move(physics2D));
		
		// Physics 3D System (priority: 200) - Legacy code still handles physics for now
		// auto physics3D = std::make_unique<PhysicsSystem3D>();
		// physics3D->OnAttach(m_Context);
		// m_Systems.push_back(std::move(physics3D));
		
		// Render System (priority: 1000) - Legacy rendering still active
		// auto renderSystem = std::make_unique<SceneRenderSystem>();
		// renderSystem->OnAttach(m_Context);
		// m_Systems.push_back(std::move(renderSystem));
		
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
		
		// Start AAA systems (includes ScriptSystem)
		for (auto& system : m_Systems) {
			if (system->IsEnabled()) {
				system->OnRuntimeStart(SceneMode::Play);
			}
		}
		
		// Legacy physics code (will be replaced when PhysicsSystem2D/3D are enabled)
		OnPhysics2DStart();
		OnPhysics3DStart();
	}

	void Scene::OnRuntimeStop() {
		// Legacy physics code
		OnPhysics3DStop();
		OnPhysics2DStop();
		
		// Stop AAA systems
		for (auto& system : m_Systems) {
			system->OnRuntimeStop();
		}
		
		m_Context.Mode = SceneMode::Edit;
	}

	void Scene::OnSimulationStart() {
		m_Context.Mode = SceneMode::Simulate;
		
		// Start AAA systems
		for (auto& system : m_Systems) {
			if (system->IsEnabled()) {
				system->OnRuntimeStart(SceneMode::Simulate);
			}
		}
		
		// Legacy physics code
		OnPhysics2DStart();
		OnPhysics3DStart();
	}

	void Scene::OnSimulationStop() {
		// Legacy physics code
		OnPhysics3DStop();
		OnPhysics2DStop();
		
		// Stop AAA systems
		for (auto& system : m_Systems) {
			system->OnRuntimeStop();
		}
		
		m_Context.Mode = SceneMode::Edit;
	}

	void Scene::OnUpdateRuntime(Timestep ts) {
		// Update AAA systems (includes ScriptSystem for scripts)
		UpdateSystems(ts, SceneMode::Play);

		// ========================================
		// PARALLELIZED PHYSICS 2D (Legacy - will be replaced by PhysicsSystem2D)
		// ========================================
		{
			// ✅ Box2D v3.x: Step con firma actualizada
			b2World_Step(m_PhysicsWorld, ts, 4);

			// ✅ PARALLEL: Retrieve transforms from Box2D across multiple threads
			auto view = m_Registry.view<Rigidbody2DComponent>();
			std::vector<entt::entity> entities(view.begin(), view.end());

			if (!entities.empty()) {
				// Use ParallelFor to distribute entity updates across workers
				auto counter = JobSystem::Get().ParallelFor(
					0,
					static_cast<uint32_t>(entities.size()),
					[this, &entities](uint32_t index) {
						entt::entity e = entities[index];
						Entity entity = { e, this };

						// Check if entity still has required components (thread-safe read)
						if (!entity.HasComponent<TransformComponent>() || !entity.HasComponent<Rigidbody2DComponent>()) {
							return;
						}

						auto& transform = entity.GetComponent<TransformComponent>();
						auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

						b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
						b2Vec2 position = b2Body_GetPosition(bodyId);
						b2Rot rotation = b2Body_GetRotation(bodyId);

						transform.Translation.x = position.x;
						transform.Translation.y = position.y;
						transform.Rotation.z = b2Rot_GetAngle(rotation);
					},
					64,  // Grain size: process 64 entities per job
					JobPriority::High
				);

				// Wait for all physics updates to complete
				JobSystem::Get().Wait(counter);
			}
		}

		// ========================================
		// PARALLELIZED PHYSICS 3D
		// ========================================
		{
			// ✅ CRITICAL FIX: Use proper substeps and fixed timestep
			float fixedTimeStep = 1.0f / 60.0f; // 60 FPS physics
			int maxSubSteps = 30;

			PhysicsCore::Get().GetWorld()->StepSimulation(ts, maxSubSteps, fixedTimeStep);

			// ✅ PARALLEL: Retrieve transforms from Bullet across multiple threads
			auto view = m_Registry.view<TransformComponent, Rigidbody3DComponent>();
			std::vector<entt::entity> entities(view.begin(), view.end());

			if (!entities.empty()) {
				auto counter = JobSystem::Get().ParallelFor(
					0,
					static_cast<uint32_t>(entities.size()),
					[this, &entities, &view](uint32_t index) {
						entt::entity e = entities[index];

						// Check if entity still valid (might have been destroyed)
						if (!m_Registry.valid(e)) {
							return;
						}

						// Get components (thread-safe if only reading/writing own entity)
						auto& transform = view.get<TransformComponent>(e);
						auto& rb3d = view.get<Rigidbody3DComponent>(e);

						if (rb3d.RuntimeBody) {
							RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);

							// ✅ NEW: Clamp velocity for heavy objects to prevent tunneling
							if (rb3d.Mass > 10.0f) {
								glm::vec3 velocity = body->GetLinearVelocity();
								float speed = glm::length(velocity);

								// Max speed based on mass (heavier = slower max speed)
								float maxSpeed = 50.0f / std::sqrt(rb3d.Mass / 10.0f);

								if (speed > maxSpeed) {
									glm::vec3 clampedVelocity = glm::normalize(velocity) * maxSpeed;
									body->SetLinearVelocity(clampedVelocity);
								}
							}

							// Get position and rotation from physics
							glm::vec3 position = body->GetPosition();
							glm::quat rotation = body->GetRotation();

							// Update transform
							transform.Translation = position;
							transform.Rotation = glm::eulerAngles(rotation);
						}
					},
					32,  // Grain size: process 32 3D physics entities per job
					JobPriority::High
				);

				// Wait for all 3D physics updates to complete
				JobSystem::Get().Wait(counter);
			}
		}

		// Render con la cámara principal de la escena
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
			// ========================================
			// RENDER GLOBAL SKYBOX (Runtime)
			// ========================================
			SkyboxRenderer::RenderGlobalSkybox(*mainCamera, cameraTransform);

			// Render 2D
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Draw sprites
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group) {
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
					Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
				}
			}

			// Draw circles
			{
				auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
				for (auto entity : view) {
					auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
					Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
				}
			}

			Renderer2D::EndScene();

			
			// Render 3D meshes
			Renderer3D::BeginScene(*mainCamera, cameraTransform);

			// Update lights for rendering (lighting affects meshes, but icons are NOT rendered)
			Renderer3D::UpdateLights(this);

			{
				auto view = m_Registry.view<TransformComponent, MeshComponent>();
				for (auto entity : view) {
					Entity e = { entity, this };
					auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);

					if (e.HasComponent<MaterialComponent>()) {
						auto& material = e.GetComponent<MaterialComponent>();
						Renderer3D::DrawMesh(transform.GetTransform(), mesh, material, (int)entity);
					}
					else {
						Renderer3D::DrawMesh(transform.GetTransform(), mesh, (int)entity);
					}
				}
			}

			Renderer3D::EndScene();
		}
	}

	void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& camera) {
		// Update AAA systems
		UpdateSystems(ts, SceneMode::Simulate);
		
		// Physics 2D
		{
			// ✅ Box2D v3.x: Step con firma actualizada
			b2World_Step(m_PhysicsWorld, ts, 4);

			// Retrieve transform from Box2D
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view) {
				Entity entity = { e, this };
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

				b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
				b2Vec2 position = b2Body_GetPosition(bodyId);
				b2Rot rotation = b2Body_GetRotation(bodyId);

				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = b2Rot_GetAngle(rotation);
			}
		}

		// Physics 3D
		{
			// ✅ CRITICAL FIX: Use proper substeps and fixed timestep
			float fixedTimeStep = 1.0f / 60.0f;
			int maxSubSteps = 30;

			PhysicsCore::Get().GetWorld()->StepSimulation(ts, maxSubSteps, fixedTimeStep);

			// Retrieve transforms from Bullet
			auto view = m_Registry.view<TransformComponent, Rigidbody3DComponent>();
			for (auto e : view) {
				auto& transform = view.get<TransformComponent>(e);
				auto& rb3d = view.get<Rigidbody3DComponent>(e);

				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);

					// ✅ NEW: Clamp velocity for heavy objects
					if (rb3d.Mass > 10.0f) {
						glm::vec3 velocity = body->GetLinearVelocity();
						float speed = glm::length(velocity);

						float maxSpeed = 50.0f / std::sqrt(rb3d.Mass / 10.0f);

						if (speed > maxSpeed) {
							glm::vec3 clampedVelocity = glm::normalize(velocity) * maxSpeed;
							body->SetLinearVelocity(clampedVelocity);
						}
					}

					// Get position and rotation from physics
					glm::vec3 position = body->GetPosition();
					glm::quat rotation = body->GetRotation();

					// Update transform
					transform.Translation = position;
					transform.Rotation = glm::eulerAngles(rotation);
				}
			}
		}

		// Render
		RenderScene(camera);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
		// Update AAA systems (only those active in Edit mode)
		UpdateSystems(ts, SceneMode::Edit);
		
		// Render
		RenderScene(camera);
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

	void Scene::OnPhysics2DStart() {
		// ✅ Box2D v3.x: Crear mundo con b2WorldDef
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { 0.0f, -9.8f };
		m_PhysicsWorld = b2CreateWorld(&worldDef);

		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view) {
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			// ✅ Box2D v3.x: Crear body con b2BodyDef
			b2BodyDef bodyDef = b2DefaultBodyDef();
			bodyDef.type = Rigidbody2DTypeToBox2DBody(rb2d.Type);
			bodyDef.position = { transform.Translation.x, transform.Translation.y };
			bodyDef.rotation = b2MakeRot(transform.Rotation.z);
			bodyDef.enableSleep = !rb2d.FixedRotation;

			b2BodyId bodyId = b2CreateBody(m_PhysicsWorld, &bodyDef);

			if (rb2d.FixedRotation) {
				b2Body_SetAngularVelocity(bodyId, 0.0f);
			}

			rb2d.RuntimeBody = new b2BodyId(bodyId);

			// BoxCollider2D
			if (entity.HasComponent<BoxCollider2DComponent>()) {
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2Polygon boxShape = b2MakeBox(
					bc2d.Size.x * transform.Scale.x,
					bc2d.Size.y * transform.Scale.y
				);

				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = bc2d.Density;
				shapeDef.material.friction = bc2d.Friction;
				shapeDef.material.restitution = bc2d.Restitution;

				b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
				bc2d.RuntimeFixture = new b2ShapeId(shapeId);
			}

			// CircleCollider2D
			if (entity.HasComponent<CircleCollider2DComponent>()) {
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2Circle circleShape;
				circleShape.center = { cc2d.Offset.x, cc2d.Offset.y };
				circleShape.radius = cc2d.Radius * transform.Scale.x;

				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = cc2d.Density;
				shapeDef.material.friction = cc2d.Friction;
				shapeDef.material.restitution = cc2d.Restitution;

				b2ShapeId shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circleShape);
				cc2d.RuntimeFixture = new b2ShapeId(shapeId);
			}
		}
	}

	void Scene::OnPhysics2DStop() {
		// ✅ Limpiar recursos antes de destruir el mundo
		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view) {
			Entity entity = { e, this };
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			if (rb2d.RuntimeBody) {
				delete static_cast<b2BodyId*>(rb2d.RuntimeBody);
				rb2d.RuntimeBody = nullptr;
			}

			if (entity.HasComponent<BoxCollider2DComponent>()) {
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
				if (bc2d.RuntimeFixture) {
					delete static_cast<b2ShapeId*>(bc2d.RuntimeFixture);
					bc2d.RuntimeFixture = nullptr;
				}
			}

			if (entity.HasComponent<CircleCollider2DComponent>()) {
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
				if (cc2d.RuntimeFixture) {
					delete static_cast<b2ShapeId*>(cc2d.RuntimeFixture);
					cc2d.RuntimeFixture = nullptr;
				}
			}
		}

		// ✅ Box2D v3.x: Destruir mundo
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			b2DestroyWorld(m_PhysicsWorld);
			m_PhysicsWorld = B2_NULL_ID;
		}
	}

	void Scene::RenderScene(EditorCamera& camera) {

		// ========================================
		// RENDER GLOBAL SKYBOX FIRST (controlled by SettingsPanel)
		// ========================================
		SkyboxRenderer::RenderGlobalSkybox(camera);

		Renderer2D::BeginScene(camera);

		// ========================================
		// RENDER INFINITE GRID (after BeginScene, before entities)
		// ========================================
		GridRenderer::DrawGrid(camera);

		// Draw sprites
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group) {
				Entity e = { entity, this };
				auto& sprite = group.get<SpriteRendererComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);
				Renderer2D::DrawSprite(worldTransform, sprite, (int)entity);
			}
		}

		// Draw circles
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& circle = view.get<CircleRendererComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);
				Renderer2D::DrawCircle(worldTransform, circle.Color, circle.Thickness, circle.Fade, (int)entity);
			}
		}

		// End current batch before rendering billboards to avoid texture slot conflicts
		Renderer2D::EndScene();

		// Start fresh batch for billboards and frustums
		Renderer2D::BeginScene(camera);

		// Render billboards (lights and cameras) sorted by distance
		struct BillboardData {
			glm::vec3 Position;
			Ref<Texture2D> Texture;
			int EntityID;
			float Distance;
			float Size;
			int Priority;
		};

		std::vector<BillboardData> billboards;
		glm::vec3 cameraPos = camera.GetPosition();

		// Collect light billboards
		{
			auto view = m_Registry.view<TransformComponent, LightComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& light = view.get<LightComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);
				glm::vec3 worldPos = glm::vec3(worldTransform[3]);

				if (light.IconTexture && light.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPos - worldPos);
					billboards.push_back({ worldPos, light.IconTexture, (int)entity, distance, 0.5f, 0 });
				}
			}
		}

		// Collect camera billboards
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& cameraComp = view.get<CameraComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);
				glm::vec3 worldPos = glm::vec3(worldTransform[3]);

				if (cameraComp.IconTexture && cameraComp.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPos - worldPos);
					glm::vec3 toCamera = glm::normalize(cameraPos - worldPos);
					glm::vec3 offsetPos = worldPos + toCamera * 0.1f;
					billboards.push_back({ offsetPos, cameraComp.IconTexture, (int)entity, distance, 1.0f, 1 });
				}
			}
		}

		// Sort billboards
		std::sort(billboards.begin(), billboards.end(),
			[](const BillboardData& a, const BillboardData& b) {
				if (std::abs(a.Distance - b.Distance) < 0.01f) {
					return a.Priority < b.Priority;
				}
				return a.Distance > b.Distance;
			});

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetDepthMask(false);
		}

		for (const auto& billboard : billboards) {
			Renderer2D::DrawBillboard(billboard.Position, billboard.Texture,
				cameraPos, billboard.Size, billboard.EntityID);
		}

		if (cmdList) {
			cmdList->SetDepthMask(true);
		}

		// ========================================
		// DRAW CAMERA FRUSTUMS (Editor only)
		// ========================================
		float previousLineWidth = Renderer2D::GetLineWidth();
		Renderer2D::SetLineWidth(0.15f);

		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& cameraComp = view.get<CameraComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);

				glm::mat4 cameraProjection = cameraComp.Camera.GetProjection();
				glm::mat4 cameraView = glm::inverse(worldTransform);

				glm::vec4 frustumColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				Renderer2D::DrawCameraFrustum(cameraProjection, cameraView, frustumColor, (int)entity);
			}
		}

		// ========================================
		// DRAW LIGHT GIZMOS (Editor only)
		// ========================================
		{
			auto view = m_Registry.view<TransformComponent, LightComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& light = view.get<LightComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);
				glm::vec3 worldPos = glm::vec3(worldTransform[3]);

				glm::vec4 gizmoColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				glm::vec3 forward = -glm::normalize(glm::vec3(worldTransform[2]));

				switch (light.GetType()) {
				case LightType::Point:
					Renderer2D::DrawPointLightGizmo(worldPos, light.GetRange(), gizmoColor, (int)entity);
					break;
				case LightType::Directional:
					Renderer2D::DrawDirectionalLightGizmo(worldPos, forward, gizmoColor, (int)entity);
					break;
				case LightType::Spot:
					Renderer2D::DrawSpotLightGizmo(worldPos, forward, light.GetRange(), light.GetOuterConeAngle(), gizmoColor, (int)entity);
					break;
				}
			}
		}

		Renderer2D::SetLineWidth(previousLineWidth);
		Renderer2D::EndScene();

		// Render 3D meshes
		Renderer3D::BeginScene(camera);
		Renderer3D::UpdateLights(this);

		{
			auto view = m_Registry.view<TransformComponent, MeshComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& mesh = view.get<MeshComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(e);

				if (e.HasComponent<MaterialComponent>()) {
					auto& material = e.GetComponent<MaterialComponent>();
					Renderer3D::DrawMesh(worldTransform, mesh, material, (int)entity);
				}
				else {
					Renderer3D::DrawModel(worldTransform, mesh.MeshModel, mesh.Color, (int)entity);
				}
			}
		}

		Renderer3D::EndScene();
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

	template<> void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& component) {
		// Auto-create MaterialComponent when MeshComponent is added
		if (!entity.HasComponent<MaterialComponent>()) {
			entity.AddComponent<MaterialComponent>();
		}
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
		// No hacer nada especial al añadir el componente
		// La compilación y carga se manejará en ScriptingEngine
	}

	// ========================================
	// 3D PHYSICS (Bullet3)
	// ========================================

	void Scene::OnPhysics3DStart() {
		// Initialize Bullet physics
		PhysicsConfig config;
		config.Gravity = glm::vec3(0.0f, -9.81f, 0.0f);
		config.FixedTimestep = 1.0f / 60.0f;
		PhysicsCore::Get().Initialize(config);

		// Create rigid bodies for all entities with Rigidbody3DComponent
		auto view = m_Registry.view<TransformComponent, Rigidbody3DComponent>();
		for (auto e : view) {
			auto& transform = view.get<TransformComponent>(e);
			auto& rb3d = view.get<Rigidbody3DComponent>(e);

			// Determine collider type and apply scale
			ColliderComponent* collider = new ColliderComponent();

			if (m_Registry.all_of<BoxCollider3DComponent>(e)) {
				auto& bc3d = m_Registry.get<BoxCollider3DComponent>(e);
				// ✅ Apply scale to half extents
				glm::vec3 scaledHalfExtents = bc3d.HalfExtents * transform.Scale;
				collider->CreateBoxShape(scaledHalfExtents);
				collider->SetOffset(bc3d.Offset);
			}
			else if (m_Registry.all_of<SphereCollider3DComponent>(e)) {
				auto& sc3d = m_Registry.get<SphereCollider3DComponent>(e);
				// ✅ Apply uniform scale (use max component for sphere)
				float scaledRadius = sc3d.Radius * std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
				collider->CreateSphereShape(scaledRadius);
				collider->SetOffset(sc3d.Offset);
			}
			else if (m_Registry.all_of<CapsuleCollider3DComponent>(e)) {
				auto& cc3d = m_Registry.get<CapsuleCollider3DComponent>(e);
				// ✅ Apply scale to radius and height
				float scaledRadius = cc3d.Radius * std::max(transform.Scale.x, transform.Scale.z);
				float scaledHeight = cc3d.Height * transform.Scale.y;
				collider->CreateCapsuleShape(scaledRadius, scaledHeight);
				collider->SetOffset(cc3d.Offset);
			}
			else {
				// Default box collider
				collider->CreateBoxShape(glm::vec3(0.5f) * transform.Scale);
			}

			rb3d.RuntimeCollider = collider;

			// Create rigid body
			PhysicsMaterial material;
			material.Mass = rb3d.Mass;
			material.Friction = rb3d.Friction;
			material.Restitution = rb3d.Restitution;
			material.LinearDamping = rb3d.LinearDamping;
			material.AngularDamping = rb3d.AngularDamping;
			material.IsStatic = (rb3d.Type == Rigidbody3DComponent::BodyType::Static);
			material.IsKinematic = (rb3d.Type == Rigidbody3DComponent::BodyType::Kinematic);
			material.IsTrigger = rb3d.IsTrigger;
			material.UseCCD = rb3d.UseCCD;
			material.CcdMotionThreshold = rb3d.CcdMotionThreshold;
			material.CcdSweptSphereRadius = rb3d.CcdSweptSphereRadius;

			glm::quat rotation = glm::quat(transform.Rotation);

			RigidBodyComponent* body = new RigidBodyComponent();
			body->Create(
				PhysicsCore::Get().GetWorld(),
				collider,
				material,
				transform.Translation,
				rotation
			);

			// Apply constraints
			body->SetLinearFactor(rb3d.LinearFactor);
			body->SetAngularFactor(rb3d.AngularFactor);

			// Store user pointer for collision callbacks
			body->GetRigidBody()->setUserPointer(reinterpret_cast<void*>(static_cast<intptr_t>(e)));

			rb3d.RuntimeBody = body;
		}
	}

	void Scene::OnPhysics3DStop() {
		// Destroy all rigid bodies
		auto view = m_Registry.view<Rigidbody3DComponent>();
		for (auto e : view) {
			auto& rb3d = view.get<Rigidbody3DComponent>(e);

			if (rb3d.RuntimeBody) {
				RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
				body->Destroy(PhysicsCore::Get().GetWorld());
				delete body;
				rb3d.RuntimeBody = nullptr;
			}

			if (rb3d.RuntimeCollider) {
				delete static_cast<ColliderComponent*>(rb3d.RuntimeCollider);
				rb3d.RuntimeCollider = nullptr;
			}
		}

		// Shutdown physics
		PhysicsCore::Get().Shutdown();
	}

	// Template specializations for 3D physics components
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

		// Guardar la posición mundial actual del hijo antes de cambiar el parent
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

			// Ajustar la posición local del hijo para que mantenga su posición mundial
			if (child.HasComponent<TransformComponent>() && parent.HasComponent<TransformComponent>()) {
				glm::vec3 parentWorldPos = glm::vec3(GetWorldTransform(parent)[3]);
				// Nueva posición local = posición mundial anterior - posición mundial del padre
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

		// Guardar posición mundial actual antes de desemparentar
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

		// restaurar posición mundial como posición local (ya no tiene padre)
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
			// Solo heredar la posición del padre, no rotación/escala
			auto& parentTransform = parent.GetComponent<TransformComponent>();
			glm::vec3 parentWorldPos = glm::vec3(GetWorldTransform(parent)[3]);
			
			// Aplicar offset de posición del padre
			glm::mat4 parentOffset = glm::translate(glm::mat4(1.0f), parentWorldPos);
			return parentOffset * localTransform;
		}
		
		return localTransform;
	}
}