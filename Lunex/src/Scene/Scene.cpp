#include "stpch.h"
#include "Scene.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Core/Input.h"

#include "../../Lunex-ScriptCore/src/ScriptPlugin.h"
#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "Entity.h"

#include <filesystem>
#include <fstream>

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

	// Variables globales para el sistema de scripting (lambdas sin captura)
	static float g_CurrentDeltaTime = 0.016f;
	static Scene* g_CurrentScene = nullptr;

	Scene::Scene() {
		InitializeEngineContext();
	}

	Scene::~Scene() {
		// Limpiar puntero global si somos la scene activa
		if (g_CurrentScene == this) {
			g_CurrentScene = nullptr;
		}
		
		// Cleanup scripts
		OnScriptsStop();
		
		// Box2D v3.x: verificar si el mundo existe antes de destruir
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			b2DestroyWorld(m_PhysicsWorld);
		}
	}

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

		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

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

		return entity;
	}

	void Scene::DestroyEntity(Entity entity) {
		m_Registry.destroy(entity);
	}

	void Scene::OnRuntimeStart() {
		OnPhysics2DStart();
		OnScriptsStart();
	}

	void Scene::OnRuntimeStop() {
		OnScriptsStop();
		OnPhysics2DStop();
	}

	void Scene::OnSimulationStart() {
		OnPhysics2DStart();
	}

	void Scene::OnSimulationStop() {
		OnPhysics2DStop();
	}

	void Scene::OnUpdateRuntime(Timestep ts) {
		// Update scripts con deltaTime real
		OnScriptsUpdate(ts);
		
		// Native scripts
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc) {
				if (!nsc.Instance) {
					nsc.Instance = nsc.InstantiateScript();
					nsc.Instance->m_Entity = Entity{ entity, this };
					nsc.Instance->OnCreate();
				}
				nsc.Instance->OnUpdate(ts);
				});
		}

		// Physics
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

			// End current batch before rendering billboards to avoid texture slot conflicts
			Renderer2D::EndScene();
			
			// Start fresh batch for billboards
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Render billboards (lights and cameras) sorted by distance
			struct BillboardData {
				glm::vec3 Position;
				Ref<Texture2D> Texture;
				int EntityID;
				float Distance;
				float Size;
				int Priority; // 0 = luz (primero), 1 = cámara (después)
			};
			
			std::vector<BillboardData> billboards;
			// Extract camera position from transform matrix
			glm::vec3 cameraPos = glm::vec3(cameraTransform[3]);
			
			// Collect light billboards
			{
				auto view = m_Registry.view<TransformComponent, LightComponent>();
				for (auto entity : view) {
					auto [transform, light] = view.get<TransformComponent, LightComponent>(entity);
					
					// Only add billboard if texture is valid and loaded
					if (light.IconTexture && light.IconTexture->IsLoaded()) {
						float distance = glm::length(cameraPos - transform.Translation);
						billboards.push_back({ transform.Translation, light.IconTexture, (int)entity, distance, 0.5f, 0 });
					}
				}
			}
			
			// Collect camera billboards
			{
				auto view = m_Registry.view<TransformComponent, CameraComponent>();
				for (auto entity : view) {
					auto [transform, cameraComp] = view.get<TransformComponent, CameraComponent>(entity);
					
					// Only add billboard if texture is valid and loaded
					if (cameraComp.IconTexture && cameraComp.IconTexture->IsLoaded()) {
						float distance = glm::length(cameraPos - transform.Translation);
						// Offset slightly towards camera to avoid z-fighting
						glm::vec3 offsetPos = transform.Translation + glm::normalize(cameraPos - transform.Translation) * 0.01f;
						billboards.push_back({ offsetPos, cameraComp.IconTexture, (int)entity, distance, 1.0f, 1 });
					}
				}
			}
			
			// Sort billboards: first by distance (farthest first), then by priority
			std::sort(billboards.begin(), billboards.end(), 
				[](const BillboardData& a, const BillboardData& b) {
					if (std::abs(a.Distance - b.Distance) < 0.01f) {
						return a.Priority < b.Priority; // Luces antes que cámaras si están en la misma posición
					}
					return a.Distance > b.Distance; // Farthest first
				});
			
			// Disable depth writing for billboards (but keep depth testing)
			RenderCommand::SetDepthMask(false);
			
			// Render sorted billboards
			for (const auto& billboard : billboards) {
				Renderer2D::DrawBillboard(billboard.Position, billboard.Texture,
					cameraPos, billboard.Size, billboard.EntityID);
			}
			
			// Re-enable depth writing
			RenderCommand::SetDepthMask(true);

			Renderer2D::EndScene();
			
			// Render 3D (SIN iconos de luz en runtime)
			Renderer3D::BeginScene(*mainCamera, cameraTransform);

			// Update lights before rendering meshes (para iluminación, pero iconos NO se renderizan)
			Renderer3D::UpdateLights(this);

			{
				auto view = m_Registry.view<TransformComponent, MeshComponent>();
				for (auto entity : view) {
					Entity e = { entity, this };
					auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);

					// Check for TextureComponent and MaterialComponent
					if (e.HasComponent<TextureComponent>() && e.HasComponent<MaterialComponent>()) {
						auto& texture = e.GetComponent<TextureComponent>();
						auto& material = e.GetComponent<MaterialComponent>();
						Renderer3D::DrawMesh(transform.GetTransform(), mesh, material, texture, (int)entity);
					}
					// Use MaterialComponent if available
					else if (e.HasComponent<MaterialComponent>()) {
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
		// Physics
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

		// Render
		RenderScene(camera);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
		// Render
		RenderScene(camera);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height) {
		m_ViewportWidth = width;
		m_ViewportHeight = height;

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
		Renderer2D::BeginScene(camera);

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

		// End current batch before rendering billboards to avoid texture slot conflicts
		Renderer2D::EndScene();
		
		// Start fresh batch for billboards
		Renderer2D::BeginScene(camera);

		// Render billboards (lights and cameras) sorted by distance
		struct BillboardData {
			glm::vec3 Position;
			Ref<Texture2D> Texture;
			int EntityID;
			float Distance;
			float Size;
			int Priority; // 0 = luz (primero), 1 = cámara (después)
		};
		
		std::vector<BillboardData> billboards;
		glm::vec3 cameraPos = camera.GetPosition();
		
		// Collect light billboards
		{
			auto view = m_Registry.view<TransformComponent, LightComponent>();
			for (auto entity : view) {
				auto [transform, light] = view.get<TransformComponent, LightComponent>(entity);
				
				// Only add billboard if texture is valid and loaded
				if (light.IconTexture && light.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPos - transform.Translation);
					billboards.push_back({ transform.Translation, light.IconTexture, (int)entity, distance, 0.5f, 0 });
				}
			}
		}
		
		// Collect camera billboards
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view) {
				auto [transform, cameraComp] = view.get<TransformComponent, CameraComponent>(entity);
				
				// Only add billboard if texture is valid and loaded
				if (cameraComp.IconTexture && cameraComp.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPos - transform.Translation);
					// Offset slightly towards camera to avoid z-fighting
					glm::vec3 offsetPos = transform.Translation + glm::normalize(cameraPos - transform.Translation) * 0.01f;
					billboards.push_back({ offsetPos, cameraComp.IconTexture, (int)entity, distance, 1.0f, 1 });
				}
			}
		}
		
		// Sort billboards: first by distance (farthest first), then by priority
		std::sort(billboards.begin(), billboards.end(), 
			[](const BillboardData& a, const BillboardData& b) {
				if (std::abs(a.Distance - b.Distance) < 0.01f) {
					return a.Priority < b.Priority; // Luces antes que cámaras si están en la misma posición
				}
				return a.Distance > b.Distance; // Farthest first
			});
		
		// Disable depth writing for billboards (but keep depth testing)
		RenderCommand::SetDepthMask(false);
		
		// Render sorted billboards
		for (const auto& billboard : billboards) {
			Renderer2D::DrawBillboard(billboard.Position, billboard.Texture,
				cameraPos, billboard.Size, billboard.EntityID);
		}
		
		// Re-enable depth writing
		RenderCommand::SetDepthMask(true);

		Renderer2D::EndScene();

		// Render 3D meshes
		Renderer3D::BeginScene(camera);

		// Update lights before rendering meshes
		Renderer3D::UpdateLights(this);

		{
			auto view = m_Registry.view<TransformComponent, MeshComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);

				// Check for TextureComponent and MaterialComponent
				if (e.HasComponent<TextureComponent>() && e.HasComponent<MaterialComponent>()) {
					auto& texture = e.GetComponent<TextureComponent>();
					auto& material = e.GetComponent<MaterialComponent>();
					Renderer3D::DrawMesh(transform.GetTransform(), mesh, material, texture, (int)entity);
				}
				// Use MaterialComponent if available
				else if (e.HasComponent<MaterialComponent>()) {
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

	void Scene::DuplicateEntity(Entity entity) {
		Entity newEntity = CreateEntity(entity.GetName());
		CopyComponentIfExists(AllComponents{}, newEntity, entity);
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
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
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
		// La compilación y carga se manejará en OnScriptsStart
	}

	// ============================================================================
	// SCRIPTING SYSTEM IMPLEMENTATION
	// ============================================================================

	void Scene::InitializeEngineContext() {
		m_EngineContext = std::make_unique<EngineContext>();
		
		// Guardar puntero a esta scene globalmente
		g_CurrentScene = this;

		// === LOGGING ===
		m_EngineContext->LogInfo = [](const char* message) {
			LNX_LOG_INFO("[Script] {0}", message);
		};

		m_EngineContext->LogWarning = [](const char* message) {
			LNX_LOG_WARN("[Script] {0}", message);
		};

		m_EngineContext->LogError = [](const char* message) {
			LNX_LOG_ERROR("[Script] {0}", message);
		};

		// === TIME ===
		m_EngineContext->GetDeltaTime = []() -> float {
			return g_CurrentDeltaTime;
		};

		m_EngineContext->GetTime = []() -> float {
			return (float)glfwGetTime();
		};

		// === ENTITY MANAGEMENT ===
		m_EngineContext->CreateEntity = [](const char* name) -> void* {
			if (!g_CurrentScene) return nullptr;
			Entity newEntity = g_CurrentScene->CreateEntity(name);
			auto entityHandle = (entt::entity)newEntity;
			return reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));
		};

		m_EngineContext->DestroyEntity = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			g_CurrentScene->DestroyEntity(ent);
		};
		
		// === TRANSFORM COMPONENT ===
		m_EngineContext->GetEntityPosition = [](void* entity, Vec3* outPosition) {
			if (!entity || !g_CurrentScene || !outPosition) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outPosition->x = transform.Translation.x;
				outPosition->y = transform.Translation.y;
				outPosition->z = transform.Translation.z;
			}
		};

		m_EngineContext->SetEntityPosition = [](void* entity, const Vec3* position) {
			if (!entity || !g_CurrentScene || !position) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Translation.x = position->x;
				transform.Translation.y = position->y;
				transform.Translation.z = position->z;
			}
		};

		m_EngineContext->GetEntityRotation = [](void* entity, Vec3* outRotation) {
			if (!entity || !g_CurrentScene || !outRotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outRotation->x = transform.Rotation.x;
				outRotation->y = transform.Rotation.y;
				outRotation->z = transform.Rotation.z;
			}
		};

		m_EngineContext->SetEntityRotation = [](void* entity, const Vec3* rotation) {
			if (!entity || !g_CurrentScene || !rotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Rotation.x = rotation->x;
				transform.Rotation.y = rotation->y;
				transform.Rotation.z = rotation->z;
			}
		};

		m_EngineContext->GetEntityScale = [](void* entity, Vec3* outScale) {
			if (!entity || !g_CurrentScene || !outScale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outScale->x = transform.Scale.x;
				outScale->y = transform.Scale.y;
				outScale->z = transform.Scale.z;
			}
		};

		m_EngineContext->SetEntityScale = [](void* entity, const Vec3* scale) {
			if (!entity || !g_CurrentScene || !scale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Scale.x = scale->x;
				transform.Scale.y = scale->y;
				transform.Scale.z = scale->z;
			}
		};
		
		// === INPUT SYSTEM ===
		m_EngineContext->IsKeyPressed = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyDown = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyReleased = [](int keyCode) -> bool {
			return !Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsMouseButtonPressed = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonDown = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonReleased = [](int button) -> bool {
			return !Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->GetMousePosition = [](float* outX, float* outY) {
			if (!outX || !outY) return;
			auto pos = Input::GetMousePosition();
			*outX = pos.first;
			*outY = pos.second;
		};

		m_EngineContext->GetMouseX = []() -> float {
			return Input::GetMouseX();
		};

		m_EngineContext->GetMouseY = []() -> float {
			return Input::GetMouseY();
		};
		
		// CurrentEntity se establecerá cuando se cargue el script
		m_EngineContext->CurrentEntity = nullptr;

		for (int i = 0; i < 16; ++i) {
			m_EngineContext->reserved[i] = nullptr;
		}
	}

	void Scene::OnScriptsStart() {
		// Iterar por todas las entidades con ScriptComponent
		auto view = m_Registry.view<ScriptComponent, IDComponent>();
		for (auto entity : view) {
			auto& scriptComp = view.get<ScriptComponent>(entity);
			auto& idComp = view.get<IDComponent>(entity);

			if (scriptComp.ScriptPath.empty()) {
				continue;
			}

			// Compilar script si es necesario
			if (scriptComp.AutoCompile) {
				std::string dllPath;
				if (CompileScript(scriptComp.ScriptPath, dllPath)) {
					scriptComp.CompiledDLLPath = dllPath;
					LNX_LOG_INFO("Script compiled successfully: {0}", dllPath);
				}
				else {
					LNX_LOG_ERROR("Failed to compile script: {0}", scriptComp.ScriptPath);
					continue;
				}
			}

			// Si no hay DLL compilada, saltar
			if (scriptComp.CompiledDLLPath.empty()) {
				LNX_LOG_WARN("No compiled DLL for script: {0}", scriptComp.ScriptPath);
				continue;
			}

			// Cargar script
			auto plugin = std::make_unique<ScriptPlugin>();
			
			// Establecer CurrentEntity antes de cargar el script
			auto entityHandle = (entt::entity)entity;
			m_EngineContext->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));
			
			if (plugin->Load(scriptComp.CompiledDLLPath, m_EngineContext.get())) {
				plugin->OnPlayModeEnter();
				scriptComp.IsLoaded = true;
				scriptComp.ScriptPluginInstance = plugin.get();
				
				// Guardar el plugin en el mapa usando el UUID de la entidad
				m_ScriptInstances[idComp.ID] = std::move(plugin);
				
				LNX_LOG_INFO("Script loaded and started: {0}", scriptComp.CompiledDLLPath);
			}
			else {
				LNX_LOG_ERROR("Failed to load script: {0}", scriptComp.CompiledDLLPath);
			}
		}
	}

	void Scene::OnScriptsStop() {
		// Detener todos los scripts
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin) {
				plugin->OnPlayModeExit();
				plugin->Unload();
			}
		}

		// Limpiar el mapa
		m_ScriptInstances.clear();

		// Marcar todos los componentes como descargados
		auto view = m_Registry.view<ScriptComponent>();
		for (auto entity : view) {
			auto& scriptComp = view.get<ScriptComponent>(entity);
			scriptComp.IsLoaded = false;
			scriptComp.ScriptPluginInstance = nullptr;
		}
	}

	void Scene::OnScriptsUpdate(float deltaTime) {
		// Actualizar deltaTime global
		g_CurrentDeltaTime = deltaTime;
		
		// Actualizar todos los scripts cargados
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin && plugin->IsLoaded()) {
				plugin->Update(deltaTime);
			}
		}
	}

	bool Scene::CompileScript(const std::string& scriptPath, std::string& outDLLPath) {
		std::filesystem::path fullScriptPath = std::filesystem::path("assets") / scriptPath;
		std::filesystem::path scriptDir = fullScriptPath.parent_path();
		std::filesystem::path scriptName = fullScriptPath.stem();

		// Detectar configuración actual
		std::string configuration;
		#ifdef LN_DEBUG
			configuration = "Debug";
		#elif defined(LN_RELEASE)
			configuration = "Release";
		#elif defined(LN_DIST)
			configuration = "Dist";
		#else
			configuration = "Debug";
		#endif

		// IMPORTANTE: La DLL debe estar en el directorio del script
		std::filesystem::path dllPath = scriptDir / "bin" / configuration / (scriptName.string() + ".dll");
		std::filesystem::path expPath = scriptDir / "bin" / configuration / (scriptName.string() + ".exp");
		std::filesystem::path libPath = scriptDir / "bin" / configuration / (scriptName.string() + ".lib");
		std::filesystem::path pdbPath = scriptDir / "bin" / configuration / (scriptName.string() + ".pdb");

		// === DESCARGAR DLL ANTIGUA SI EXISTE ===
		// Esto es crítico porque Windows no permite sobrescribir DLLs cargadas en memoria
		if (std::filesystem::exists(dllPath)) {
			auto dllTime = std::filesystem::last_write_time(dllPath);
			auto scriptTime = std::filesystem::last_write_time(fullScriptPath);
			
			if (dllTime >= scriptTime) {
				outDLLPath = dllPath.string();
				LNX_LOG_INFO("Found up-to-date compiled DLL: {0}", outDLLPath);
				return true;
			}
			
			LNX_LOG_INFO("Script has been modified, recompiling...");
			LNX_LOG_WARN("IMPORTANT: Unloading old DLL before recompilation...");
			
			// Descargar la DLL del plugin si está cargada
			auto view = m_Registry.view<ScriptComponent, IDComponent>();
			for (auto entity : view) {
				auto& scriptComp = view.get<ScriptComponent>(entity);
				auto& idComp = view.get<IDComponent>(entity);
				
				if (scriptComp.CompiledDLLPath == dllPath.string() && scriptComp.IsLoaded) {
					// Encontrar el plugin en el mapa
					auto it = m_ScriptInstances.find(idComp.ID);
					if (it != m_ScriptInstances.end() && it->second) {
						LNX_LOG_INFO("Unloading plugin for entity: {0}", (uint64_t)idComp.ID);
						it->second->OnPlayModeExit();
						it->second->Unload(); // ¡Esto descarga la DLL!
						m_ScriptInstances.erase(it);
						scriptComp.IsLoaded = false;
						scriptComp.ScriptPluginInstance = nullptr;
					}
				}
			}
			
			// Intentar borrar DLL antigua y archivos relacionados
			std::error_code ec;
			std::filesystem::remove(dllPath, ec);
			if (ec) {
				LNX_LOG_ERROR("Failed to delete old DLL: {0} (Error: {1})", dllPath.string(), ec.message());
				LNX_LOG_ERROR("The DLL might still be in use. Try stopping Play mode first.");
				return false;
			}
			
			// Borrar archivos auxiliares (.exp, .lib, .pdb)
			std::filesystem::remove(expPath, ec);
			std::filesystem::remove(libPath, ec);
			std::filesystem::remove(pdbPath, ec);
			
			LNX_LOG_INFO("Old DLL and related files removed successfully");
		}

		// === DETECTAR VISUAL STUDIO AUTOMÁTICAMENTE ===
		LNX_LOG_INFO("=== Auto-detecting Visual Studio installation ===");
		
		// Rutas comunes de Visual Studio
		std::vector<std::string> vsBasePaths = {
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
			"C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Professional",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
			// VS 2019 fallback
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Community",
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Professional",
			"C:\\Program Files\\Microsoft Visual Studio\\2019\\Enterprise",
			"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
		};

		std::filesystem::path vsPath;
		std::filesystem::path vcvarsPath;
		std::filesystem::path clPath;

		// Buscar Visual Studio
		for (const auto& basePath : vsBasePaths) {
			std::filesystem::path candidate(basePath);
			std::filesystem::path vcvarsCandidate = candidate / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
			
			if (std::filesystem::exists(vcvarsCandidate)) {
				vsPath = candidate;
				vcvarsPath = vcvarsCandidate;
				
				// Buscar cl.exe
				std::filesystem::path clCandidate = candidate / "VC" / "Tools" / "MSVC";
				if (std::filesystem::exists(clCandidate)) {
					// Buscar la versión más reciente del compilador
					for (auto& entry : std::filesystem::directory_iterator(clCandidate)) {
						if (entry.is_directory()) {
							std::filesystem::path clExe = entry.path() / "bin" / "Hostx64" / "x64" / "cl.exe";
							if (std::filesystem::exists(clExe)) {
								clPath = clExe;
								break;
							}
						}
					}
				}
				
				if (!clPath.empty()) {
					LNX_LOG_INFO("Found Visual Studio at: {0}", vsPath.string());
					LNX_LOG_INFO("Found cl.exe at: {0}", clPath.string());
					break;
				}
			}
		}

		if (clPath.empty()) {
			LNX_LOG_ERROR("Could not auto-detect Visual Studio installation!");
			LNX_LOG_ERROR("Please install Visual Studio 2022 or 2019 with C++ tools");
			LNX_LOG_ERROR("Download from: https://visualstudio.microsoft.com/downloads/");
			return false;
		}

		// === PREPARAR COMPILACIÓN ===
		LNX_LOG_INFO("=== Compiling script: {0} ===", scriptName.string());

		// Crear directorios de salida
		std::filesystem::path binDir = scriptDir / "bin" / configuration;
		std::filesystem::path objDir = scriptDir / "bin-int" / configuration;
		
		std::filesystem::create_directories(binDir);
		std::filesystem::create_directories(objDir);

		// Buscar rutas relativas a Lunex-ScriptCore desde el script
		std::filesystem::path scriptCoreInclude;
		std::filesystem::path lunexInclude;
		std::filesystem::path spdlogInclude;
		
		// Intentar encontrar Lunex-ScriptCore (buscar hacia arriba)
		std::filesystem::path searchDir = scriptDir;
		for (int i = 0; i < 10; i++) {
			std::filesystem::path candidate = searchDir / "Lunex-ScriptCore" / "src";
			if (std::filesystem::exists(candidate)) {
				scriptCoreInclude = candidate;
				lunexInclude = searchDir / "Lunex" / "src";
				spdlogInclude = searchDir / "vendor" / "spdlog" / "include";
				break;
			}
			searchDir = searchDir.parent_path();
		}

		if (scriptCoreInclude.empty()) {
			LNX_LOG_ERROR("Could not find Lunex-ScriptCore!");
			LNX_LOG_ERROR("Make sure your script is inside the Lunex project structure");
			return false;
		}

		// === CONSTRUIR COMANDO DE COMPILACIÓN ===
		// Usar un .bat temporal que configure el entorno y compile
		std::filesystem::path tempBatPath = scriptDir / "temp_compile.bat";
		
		std::ofstream batFile(tempBatPath);
		if (!batFile.is_open()) {
			LNX_LOG_ERROR("Failed to create temporary batch file");
			return false;
		}

		// Escribir script de batch que configura el entorno y compila
		batFile << "@echo off\n";
		batFile << "REM Auto-generated compile script\n";
		batFile << "call \"" << vcvarsPath.string() << "\" >nul 2>&1\n";
		batFile << "if errorlevel 1 (\n";
		batFile << "    echo ERROR: Failed to setup Visual Studio environment\n";
		batFile << "    exit /b 1\n";
		batFile << ")\n\n";
		
		// Flags del compilador
		std::string compilerFlags = "/LD /EHsc /std:c++20 /utf-8 /nologo";
		if (configuration == "Debug") {
			compilerFlags += " /MDd /Zi /Od /DLUNEX_SCRIPT_EXPORT /DLN_DEBUG";
		} else {
			compilerFlags += " /MD /O2 /DLUNEX_SCRIPT_EXPORT /DLN_RELEASE";
		}

		// Includes
		std::string includes = 
			" /I\"" + scriptCoreInclude.string() + "\"" +
			" /I\"" + lunexInclude.string() + "\"" +
			" /I\"" + spdlogInclude.string() + "\"";

		// Output paths
		std::string outputDll = "/Fe:\"" + dllPath.string() + "\"";
		std::string outputObj = "/Fo:\"" + objDir.string() + "\\\\\"";

		// Comando de compilación
		batFile << "cl.exe " << compilerFlags << includes;
		batFile << " \"" << fullScriptPath.string() << "\"";
		batFile << " " << outputDll << " " << outputObj << " 2>&1\n";
		batFile << "exit /b %errorlevel%\n";
		
		batFile.close();

		LNX_LOG_INFO("Compiling: {0}", scriptName.string());
		LNX_LOG_INFO("Output: {0}", dllPath.string());

		// Ejecutar el batch
		std::string batCommand = "\"" + tempBatPath.string() + "\" 2>&1";
		FILE* pipe = _popen(batCommand.c_str(), "r");
		if (!pipe) {
			LNX_LOG_ERROR("Failed to execute compile script");
			std::filesystem::remove(tempBatPath); // Limpiar
			return false;
		}

		char buffer[512];
		bool hadErrors = false;
		while (fgets(buffer, sizeof(buffer), pipe)) {
			std::string line = buffer;
			if (!line.empty() && line.back() == '\n') line.pop_back();
			if (!line.empty()) {
				// Detectar errores vs warnings
				if (line.find("error") != std::string::npos || 
					line.find("Error") != std::string::npos ||
					line.find("ERROR") != std::string::npos) {
					LNX_LOG_ERROR("[Compiler] {0}", line);
					hadErrors = true;
				} else if (line.find("warning") != std::string::npos || 
						   line.find("Warning") != std::string::npos) {
					LNX_LOG_WARN("[Compiler] {0}", line);
				} else if (line.find("Creating library") == std::string::npos && 
						   line.find(".exp") == std::string::npos) {
					// Omitir mensajes de "Creating library" y ".exp"
					LNX_LOG_INFO("[Compiler] {0}", line);
				}
			}
		}

		int result = _pclose(pipe);
		
		// Limpiar archivo temporal
		std::filesystem::remove(tempBatPath);
		
		if (result != 0 || hadErrors) {
			LNX_LOG_ERROR("Compilation failed with error code: {0}", result);
			return false;
		}

		// Verificar DLL generada
		if (std::filesystem::exists(dllPath)) {
			outDLLPath = dllPath.string();
			LNX_LOG_INFO("=== Script compiled successfully! ===");
			LNX_LOG_INFO("DLL created at: {0}", outDLLPath);
			return true;
		}
		else {
			LNX_LOG_ERROR("Compilation succeeded but DLL not found at: {0}", dllPath.string());
			return false;
		}
	}
}