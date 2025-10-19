#include "stpch.h"
#include "Scene.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Renderer/Renderer2D.h"

#include <glm/glm.hpp>

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
	}
	
	Scene::~Scene()  {
	}
	
	template<typename Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap) {
		auto view = src.view<Component>();
		for (auto e : view)	{
			UUID uuid = src.get<IDComponent>(e).ID;
			LNX_CORE_ASSERT(enttMap.find(uuid) != enttMap.end());
			entt::entity dstEnttID = enttMap.at(uuid);
			
			auto& component = src.get<Component>(e);
			dst.emplace_or_replace<Component>(dstEnttID, component);
		}
	}
	
	template<typename Component>
	static void CopyComponentIfExists(Entity dst, Entity src) {
		if (src.HasComponent<Component>())
			dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
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
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		
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
	
	void Scene::DestroyEntity(Entity entity){
		m_Registry.destroy(entity);
	}
	
	void Scene::OnRuntimeStart() {
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { 0.0f, -9.81f };
		
		m_PhysicsWorld = b2CreateWorld(&worldDef);
		
		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view) {
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			
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
			
			if (entity.HasComponent<CircleCollider2DComponent>()) {
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
				
				b2Circle circleShape;
				circleShape.center = { cc2d.Offset.x, cc2d.Offset.y };
				circleShape.radius = cc2d.Radius * transform.Scale.x; // usar escala del transform
				
				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = cc2d.Density;
				
				shapeDef.material.friction = cc2d.Friction;
				shapeDef.material.restitution = cc2d.Restitution;
				
				b2ShapeId shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circleShape);
				cc2d.RuntimeFixture = new b2ShapeId(shapeId);
			}
		}
	}
	
	void Scene::OnRuntimeStop() {
		// ✅ Limpiar los IDs almacenados antes de destruir el mundo
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
		}
		
		// ✅ DestroyWorld en v3.x
		b2DestroyWorld(m_PhysicsWorld);
		m_PhysicsWorld = B2_NULL_ID;
	}
	
	void Scene::OnUpdateRuntime(Timestep ts) {
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
				{
					//TODO: Move to Scene::OnScenePlay
					if (!nsc.Instance) {
						nsc.Instance = nsc.InstantiateScript();
						nsc.Instance->m_Entity = Entity{ entity, this };
						
						nsc.Instance->OnCreate();
					}
					
					nsc.Instance->OnUpdate(ts);
				});
		}
		
		{
			// ✅ b2World_Step en v3.x tiene diferente firma
			b2World_Step(m_PhysicsWorld, ts, 4); // subStepCount = 4 (recomendado)
			
			// Retrieve transform from Box2D
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view) {
				Entity entity = { e, this };
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				
				// ✅ Obtener el ID almacenado
				b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
				
				// ✅ Usar las nuevas funciones de acceso
				b2Vec2 position = b2Body_GetPosition(bodyId);
				b2Rot rotation = b2Body_GetRotation(bodyId);
				
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				
				// ✅ b2Rot_GetAngle extrae el ángulo de b2Rot
				transform.Rotation.z = b2Rot_GetAngle(rotation);
			}
		}
		
		// Render 2D
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
		}
	}
	
	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
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
		Renderer2D::EndScene();
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
	
	void Scene::DuplicateEntity(Entity entity) {
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);
		
		CopyComponentIfExists<TransformComponent>(newEntity, entity);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CameraComponent>(newEntity, entity);
		CopyComponentIfExists<NativeScriptComponent>(newEntity, entity);
		CopyComponentIfExists<Rigidbody2DComponent>(newEntity, entity);
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity, entity);
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
	
	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) {
		//static_assert(false);
	}
	
	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component){
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
}