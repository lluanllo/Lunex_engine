#include "stpch.h"
#include "PhysicsSystem3D.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Physics/Physics.h"
#include "Core/JobSystem/JobSystem.h"

namespace Lunex {

	PhysicsSystem3D::~PhysicsSystem3D() {
		if (m_Initialized) {
			ShutdownPhysics();
		}
	}

	// ========================================================================
	// ISceneSystem Interface
	// ========================================================================

	void PhysicsSystem3D::OnAttach(SceneContext& context) {
		m_Context = &context;
		LNX_LOG_INFO("PhysicsSystem3D attached");
	}

	void PhysicsSystem3D::OnDetach() {
		if (m_Initialized) {
			ShutdownPhysics();
		}
		m_Context = nullptr;
		LNX_LOG_INFO("PhysicsSystem3D detached");
	}

	void PhysicsSystem3D::OnRuntimeStart(SceneMode mode) {
		if (!IsActiveInMode(mode)) return;
		
		InitializePhysics();
		CreateRigidBodies();
		
		LNX_LOG_INFO("PhysicsSystem3D started (mode: {0})", SceneModeToString(mode));
	}

	void PhysicsSystem3D::OnRuntimeStop() {
		CleanupRuntimeBodies();
		ShutdownPhysics();
		
		LNX_LOG_INFO("PhysicsSystem3D stopped");
	}

	void PhysicsSystem3D::OnUpdate(Timestep ts, SceneMode mode) {
		if (!IsActiveInMode(mode) || !m_Initialized) return;
		
		// Step physics with fixed timestep
		PhysicsCore::Get().GetWorld()->StepSimulation(
			ts, 
			m_Settings.MaxSubsteps, 
			m_Settings.FixedTimestep
		);
		
		// Clamp velocities for stability
		ClampVelocities();
		
		// Sync transforms from physics
		SyncTransformsFromPhysics();
	}

	void PhysicsSystem3D::OnFixedUpdate(float fixedDeltaTime) {
		// Physics stepping is handled in OnUpdate with Bullet's internal substeps
	}

	void PhysicsSystem3D::OnSceneEvent(const SceneSystemEvent& event) {
		switch (event.Type) {
			case SceneEventType::EntityDestroyed:
				// Could cleanup physics body here if needed
				break;
			default:
				break;
		}
	}

	// ========================================================================
	// Physics3D Specific API
	// ========================================================================

	void PhysicsSystem3D::SetSettings(const Physics3DSettings& settings) {
		m_Settings = settings;
		
		if (m_Initialized) {
			PhysicsCore::Get().GetWorld()->SetGravity(m_Settings.Gravity);
		}
	}

	// ========================================================================
	// Internal Methods
	// ========================================================================

	void PhysicsSystem3D::InitializePhysics() {
		PhysicsConfig config;
		config.Gravity = m_Settings.Gravity;
		config.FixedTimestep = m_Settings.FixedTimestep;
		PhysicsCore::Get().Initialize(config);
		
		m_Initialized = true;
		m_TimeAccumulator = 0.0f;
	}

	void PhysicsSystem3D::ShutdownPhysics() {
		PhysicsCore::Get().Shutdown();
		m_Initialized = false;
	}

	void PhysicsSystem3D::CreateRigidBodies() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<TransformComponent, Rigidbody3DComponent>();
		for (auto e : view) {
			Entity entity = { e, m_Context->OwningScene };
			
			auto& transform = view.get<TransformComponent>(e);
			auto& rb3d = view.get<Rigidbody3DComponent>(e);
			
			// Create collider
			ColliderComponent* collider = new ColliderComponent();
			
			if (m_Context->Registry->all_of<BoxCollider3DComponent>(e)) {
				auto& bc3d = m_Context->Registry->get<BoxCollider3DComponent>(e);
				glm::vec3 scaledHalfExtents = bc3d.HalfExtents * transform.Scale;
				collider->CreateBoxShape(scaledHalfExtents);
				collider->SetOffset(bc3d.Offset);
			}
			else if (m_Context->Registry->all_of<SphereCollider3DComponent>(e)) {
				auto& sc3d = m_Context->Registry->get<SphereCollider3DComponent>(e);
				float scaledRadius = sc3d.Radius * std::max({ 
					transform.Scale.x, 
					transform.Scale.y, 
					transform.Scale.z 
				});
				collider->CreateSphereShape(scaledRadius);
				collider->SetOffset(sc3d.Offset);
			}
			else if (m_Context->Registry->all_of<CapsuleCollider3DComponent>(e)) {
				auto& cc3d = m_Context->Registry->get<CapsuleCollider3DComponent>(e);
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
			
			// Create physics material
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
			
			// Create rigid body
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

	void PhysicsSystem3D::SyncTransformsFromPhysics() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<TransformComponent, Rigidbody3DComponent>();
		std::vector<entt::entity> entities(view.begin(), view.end());
		
		if (entities.empty()) return;
		
		// Parallel sync
		auto counter = JobSystem::Get().ParallelFor(
			0,
			static_cast<uint32_t>(entities.size()),
			[this, &entities, &view](uint32_t index) {
				entt::entity e = entities[index];
				
				if (!m_Context->Registry->valid(e)) return;
				
				auto& transform = view.get<TransformComponent>(e);
				auto& rb3d = view.get<Rigidbody3DComponent>(e);
				
				if (!rb3d.RuntimeBody) return;
				
				RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
				
				glm::vec3 position = body->GetPosition();
				glm::quat rotation = body->GetRotation();
				
				transform.Translation = position;
				transform.Rotation = glm::eulerAngles(rotation);
			},
			32,  // Grain size
			JobPriority::High
		);
		
		JobSystem::Get().Wait(counter);
	}

	void PhysicsSystem3D::ClampVelocities() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<Rigidbody3DComponent>();
		for (auto e : view) {
			auto& rb3d = view.get<Rigidbody3DComponent>(e);
			
			if (!rb3d.RuntimeBody || rb3d.Mass <= 10.0f) continue;
			
			RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
			
			glm::vec3 velocity = body->GetLinearVelocity();
			float speed = glm::length(velocity);
			
			// Max speed based on mass
			float maxSpeed = m_Settings.MaxVelocity / std::sqrt(rb3d.Mass / 10.0f);
			
			if (speed > maxSpeed) {
				glm::vec3 clampedVelocity = glm::normalize(velocity) * maxSpeed;
				body->SetLinearVelocity(clampedVelocity);
			}
		}
	}

	void PhysicsSystem3D::CleanupRuntimeBodies() {
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<Rigidbody3DComponent>();
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
	}

} // namespace Lunex
