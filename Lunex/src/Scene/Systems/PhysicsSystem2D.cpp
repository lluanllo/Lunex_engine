#include "stpch.h"
#include "PhysicsSystem2D.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Core/JobSystem/JobSystem.h"
#include "Physics/PhysicsCore.h"

namespace Lunex {

	PhysicsSystem2D::~PhysicsSystem2D() {
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			DestroyPhysicsWorld();
		}
	}

	// ========================================================================
	// ISceneSystem Interface
	// ========================================================================

	void PhysicsSystem2D::OnAttach(SceneContext& context) {
		m_Context = &context;
		LNX_LOG_INFO("PhysicsSystem2D attached");
	}

	void PhysicsSystem2D::OnDetach() {
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			DestroyPhysicsWorld();
		}
		m_Context = nullptr;
		LNX_LOG_INFO("PhysicsSystem2D detached");
	}

	void PhysicsSystem2D::OnRuntimeStart(SceneMode mode) {
		if (!IsActiveInMode(mode)) return;
		
		CreatePhysicsWorld();
		CreateRigidBodies();
		
		LNX_LOG_INFO("PhysicsSystem2D started (mode: {0})", SceneModeToString(mode));
	}

	void PhysicsSystem2D::OnRuntimeStop() {
		CleanupRuntimeBodies();
		DestroyPhysicsWorld();
		
		LNX_LOG_INFO("PhysicsSystem2D stopped");
	}

	void PhysicsSystem2D::OnUpdate(Timestep ts, SceneMode mode) {
		if (!IsActiveInMode(mode) || !B2_IS_NON_NULL(m_PhysicsWorld)) return;
		
		// Accumulate time for fixed timestep
		m_TimeAccumulator += ts;
		
		// Run fixed updates
		int substeps = 0;
		while (m_TimeAccumulator >= m_Settings.FixedTimestep && substeps < m_Context->MaxSubsteps) {
			OnFixedUpdate(m_Settings.FixedTimestep);
			m_TimeAccumulator -= m_Settings.FixedTimestep;
			substeps++;
		}
	}

	void PhysicsSystem2D::OnFixedUpdate(float fixedDeltaTime) {
		if (!B2_IS_NON_NULL(m_PhysicsWorld)) return;
		
		// Step Box2D simulation
		b2World_Step(m_PhysicsWorld, fixedDeltaTime, m_Settings.VelocityIterations);
		
		// Sync transforms from physics
		SyncTransformsFromPhysics();
	}

	void PhysicsSystem2D::OnSceneEvent(const SceneSystemEvent& event) {
		// Handle entity/component events if needed
		switch (event.Type) {
			case SceneEventType::EntityDestroyed: {
				// Could cleanup physics body here if needed
				break;
			}
			default:
				break;
		}
	}

	// ========================================================================
	// Physics2D Specific API
	// ========================================================================

	void PhysicsSystem2D::SetSettings(const Physics2DSettings& settings) {
		m_Settings = settings;
		
		// Update world gravity if world exists
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			b2World_SetGravity(m_PhysicsWorld, { m_Settings.Gravity.x, m_Settings.Gravity.y });
		}
	}

	// ========================================================================
	// Internal Methods
	// ========================================================================

	void PhysicsSystem2D::CreatePhysicsWorld() {
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			DestroyPhysicsWorld();
		}
		
		// Read current global PhysicsCore config (set by SettingsPanel presets)
		const PhysicsConfig& globalConfig = PhysicsCore::Get().GetConfig();
		
		// Use global gravity (XY components for 2D)
		m_Settings.Gravity = glm::vec2(globalConfig.Gravity.x, globalConfig.Gravity.y);
		
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { m_Settings.Gravity.x, m_Settings.Gravity.y };
		m_PhysicsWorld = b2CreateWorld(&worldDef);
		
		m_TimeAccumulator = 0.0f;
	}

	void PhysicsSystem2D::DestroyPhysicsWorld() {
		if (B2_IS_NON_NULL(m_PhysicsWorld)) {
			b2DestroyWorld(m_PhysicsWorld);
			m_PhysicsWorld = B2_NULL_ID;
		}
	}

	void PhysicsSystem2D::CreateRigidBodies() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<Rigidbody2DComponent>();
		for (auto e : view) {
			Entity entity = { e, m_Context->OwningScene };
			
			if (!entity.HasComponent<TransformComponent>()) continue;
			
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			
			// Create body definition
			b2BodyDef bodyDef = b2DefaultBodyDef();
			bodyDef.type = ConvertBodyType(static_cast<int>(rb2d.Type));
			bodyDef.position = { transform.Translation.x, transform.Translation.y };
			bodyDef.rotation = b2MakeRot(transform.Rotation.z);
			bodyDef.enableSleep = !rb2d.FixedRotation;
			
			b2BodyId bodyId = b2CreateBody(m_PhysicsWorld, &bodyDef);
			
			if (rb2d.FixedRotation) {
				b2Body_SetAngularVelocity(bodyId, 0.0f);
			}
			
			rb2d.RuntimeBody = new b2BodyId(bodyId);
			
			// Create box collider if present
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
			
			// Create circle collider if present
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

	void PhysicsSystem2D::SyncTransformsFromPhysics() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<Rigidbody2DComponent>();
		std::vector<entt::entity> entities(view.begin(), view.end());
		
		if (entities.empty()) return;
		
		// Parallel sync for large entity counts
		auto counter = JobSystem::Get().ParallelFor(
			0,
			static_cast<uint32_t>(entities.size()),
			[this, &entities](uint32_t index) {
				entt::entity e = entities[index];
				Entity entity = { e, m_Context->OwningScene };
				
				if (!entity.HasComponent<TransformComponent>() || 
				    !entity.HasComponent<Rigidbody2DComponent>()) {
					return;
				}
				
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				
				if (!rb2d.RuntimeBody) return;
				
				b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
				b2Vec2 position = b2Body_GetPosition(bodyId);
				b2Rot rotation = b2Body_GetRotation(bodyId);
				
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = b2Rot_GetAngle(rotation);
			},
			64,  // Grain size
			JobPriority::High
		);
		
		JobSystem::Get().Wait(counter);
	}

	void PhysicsSystem2D::CleanupRuntimeBodies() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<Rigidbody2DComponent>();
		for (auto e : view) {
			Entity entity = { e, m_Context->OwningScene };
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
	}

	b2BodyType PhysicsSystem2D::ConvertBodyType(int type) {
		switch (type) {
			case 0:  return b2_staticBody;    // Static
			case 1:  return b2_dynamicBody;   // Dynamic
			case 2:  return b2_kinematicBody; // Kinematic
		}
		return b2_staticBody;
	}

} // namespace Lunex
