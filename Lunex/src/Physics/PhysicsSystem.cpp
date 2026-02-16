#include "stpch.h"
#include "PhysicsSystem.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

namespace Lunex {

    PhysicsSystem::PhysicsSystem(Scene* scene)
        : m_Scene(scene)
    {
        if (scene) {
            m_Registry = &scene->m_Registry;
        }
    }

    PhysicsSystem::~PhysicsSystem()
    {
        if (m_IsRunning)
        {
            OnRuntimeStop();
        }
    }

    void PhysicsSystem::OnRuntimeStart()
    {
        if (m_IsRunning)
            return;

        // Initialize physics core
        PhysicsConfig config;
        config.Gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.FixedTimestep = 1.0f / 60.0f;
        config.MaxSubSteps = 10;
        PhysicsCore::Get().Initialize(config);

        // Setup collision callbacks
        m_CollisionCallback.SetOnCollisionEnter([this](const CollisionEvent& event) {
            if (m_OnCollisionEnter)
            {
                entt::entity entityA = static_cast<entt::entity>(reinterpret_cast<intptr_t>(event.UserDataA));
                entt::entity entityB = static_cast<entt::entity>(reinterpret_cast<intptr_t>(event.UserDataB));
                m_OnCollisionEnter(entityA, entityB);
            }
        });

        m_CollisionCallback.SetOnCollisionExit([this](const CollisionEvent& event) {
            if (m_OnCollisionExit)
            {
                entt::entity entityA = static_cast<entt::entity>(reinterpret_cast<intptr_t>(event.UserDataA));
                entt::entity entityB = static_cast<entt::entity>(reinterpret_cast<intptr_t>(event.UserDataB));
                m_OnCollisionExit(entityA, entityB);
            }
        });

        // Create physics bodies for existing entities
        if (m_Registry) {
            auto view = m_Registry->view<TransformComponent, Rigidbody3DComponent>();
            for (auto entity : view) {
                CreatePhysicsBody(entity);
            }
        }

        m_IsRunning = true;
    }

    void PhysicsSystem::OnRuntimeStop()
    {
        if (!m_IsRunning)
            return;

        // Destroy all physics bodies
        for (auto& [entity, body] : m_EntityBodies)
        {
            if (body)
            {
                body->Destroy(PhysicsCore::Get().GetWorld());
                delete body;
            }
        }
        m_EntityBodies.clear();

        // Cleanup runtime colliders on entities
        if (m_Registry) {
            auto view = m_Registry->view<Rigidbody3DComponent>();
            for (auto e : view) {
                auto& rb3d = view.get<Rigidbody3DComponent>(e);
                if (rb3d.RuntimeCollider) {
                    delete static_cast<ColliderComponent*>(rb3d.RuntimeCollider);
                    rb3d.RuntimeCollider = nullptr;
                }
                rb3d.RuntimeBody = nullptr;
            }
        }

        PhysicsCore::Get().Shutdown();

        m_IsRunning = false;
    }

    void PhysicsSystem::OnSimulationStart()
    {
        OnRuntimeStart();
    }

    void PhysicsSystem::OnSimulationStop()
    {
        OnRuntimeStop();
    }

    void PhysicsSystem::OnUpdate(float deltaTime)
    {
        if (!m_IsRunning)
            return;

        PhysicsCore::Get().Update(deltaTime);

        SyncTransformsFromPhysics();

        ProcessCollisions();
    }

    void PhysicsSystem::OnFixedUpdate(float fixedDeltaTime)
    {
        if (!m_IsRunning)
            return;

        PhysicsCore::Get().FixedUpdate(fixedDeltaTime);

        SyncTransformsFromPhysics();

        ProcessCollisions();
    }

    void PhysicsSystem::CreatePhysicsBody(entt::entity entity)
    {
        if (!m_Registry || !m_Registry->valid(entity)) return;
        if (!m_Registry->all_of<TransformComponent, Rigidbody3DComponent>(entity)) return;

        auto& transform = m_Registry->get<TransformComponent>(entity);
        auto& rb3d = m_Registry->get<Rigidbody3DComponent>(entity);

        // Create collider based on available collider component
        ColliderComponent* collider = new ColliderComponent();

        if (m_Registry->all_of<BoxCollider3DComponent>(entity)) {
            auto& bc = m_Registry->get<BoxCollider3DComponent>(entity);
            glm::vec3 scaled = bc.HalfExtents * transform.Scale;
            collider->CreateBoxShape(scaled);
            collider->SetOffset(bc.Offset);
        }
        else if (m_Registry->all_of<SphereCollider3DComponent>(entity)) {
            auto& sc = m_Registry->get<SphereCollider3DComponent>(entity);
            float scaledR = sc.Radius * std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
            collider->CreateSphereShape(scaledR);
            collider->SetOffset(sc.Offset);
        }
        else if (m_Registry->all_of<CapsuleCollider3DComponent>(entity)) {
            auto& cc = m_Registry->get<CapsuleCollider3DComponent>(entity);
            float scaledR = cc.Radius * std::max(transform.Scale.x, transform.Scale.z);
            float scaledH = cc.Height * transform.Scale.y;
            collider->CreateCapsuleShape(scaledR, scaledH);
            collider->SetOffset(cc.Offset);
        }
        else if (m_Registry->all_of<CylinderCollider3DComponent>(entity)) {
            auto& cy = m_Registry->get<CylinderCollider3DComponent>(entity);
            glm::vec3 scaled = cy.HalfExtents * transform.Scale;
            collider->CreateCylinderShape(scaled);
            collider->SetOffset(cy.Offset);
        }
        else if (m_Registry->all_of<ConeCollider3DComponent>(entity)) {
            auto& cn = m_Registry->get<ConeCollider3DComponent>(entity);
            float scaledR = cn.Radius * std::max(transform.Scale.x, transform.Scale.z);
            float scaledH = cn.Height * transform.Scale.y;
            collider->CreateConeShape(scaledR, scaledH);
            collider->SetOffset(cn.Offset);
        }
        else if (m_Registry->all_of<MeshCollider3DComponent>(entity)) {
            auto& mc = m_Registry->get<MeshCollider3DComponent>(entity);
            if (mc.UseEntityMesh && m_Registry->all_of<MeshComponent>(entity)) {
                auto& meshComp = m_Registry->get<MeshComponent>(entity);
                if (meshComp.MeshModel) {
                    std::vector<glm::vec3> vertices;
                    std::vector<uint32_t> indices;
                    for (const auto& submesh : meshComp.MeshModel->GetMeshes()) {
                        uint32_t offset = static_cast<uint32_t>(vertices.size());
                        for (const auto& v : submesh->GetVertices()) {
                            vertices.push_back(v.Position * transform.Scale);
                        }
                        for (const auto& idx : submesh->GetIndices()) {
                            indices.push_back(idx + offset);
                        }
                    }
                    if (mc.Type == MeshCollider3DComponent::CollisionType::Convex) {
                        collider->CreateConvexHullShape(vertices);
                    } else {
                        collider->CreateTriangleMeshShape(vertices, indices);
                    }
                }
            } else if (!mc.Vertices.empty()) {
                if (mc.Type == MeshCollider3DComponent::CollisionType::Convex) {
                    collider->CreateConvexHullShape(mc.Vertices);
                } else {
                    collider->CreateTriangleMeshShape(mc.Vertices, mc.Indices);
                }
            } else {
                collider->CreateBoxShape(glm::vec3(0.5f) * transform.Scale);
            }
        }
        else {
            // Default: box collider matching scale
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
        body->GetRigidBody()->setUserPointer(reinterpret_cast<void*>(static_cast<intptr_t>(entity)));

        rb3d.RuntimeBody = body;
        m_EntityBodies[entity] = body;
    }

    void PhysicsSystem::DestroyPhysicsBody(entt::entity entity)
    {
        auto it = m_EntityBodies.find(entity);
        if (it != m_EntityBodies.end())
        {
            if (it->second)
            {
                it->second->Destroy(PhysicsCore::Get().GetWorld());
                delete it->second;
            }
            m_EntityBodies.erase(it);
        }

        // Clean up runtime collider
        if (m_Registry && m_Registry->valid(entity) && m_Registry->all_of<Rigidbody3DComponent>(entity)) {
            auto& rb3d = m_Registry->get<Rigidbody3DComponent>(entity);
            if (rb3d.RuntimeCollider) {
                delete static_cast<ColliderComponent*>(rb3d.RuntimeCollider);
                rb3d.RuntimeCollider = nullptr;
            }
            rb3d.RuntimeBody = nullptr;
        }
    }

    void PhysicsSystem::UpdatePhysicsBody(entt::entity entity)
    {
        DestroyPhysicsBody(entity);
        CreatePhysicsBody(entity);
    }

    void PhysicsSystem::SyncTransformsToPhysics()
    {
        for (auto& [entity, body] : m_EntityBodies)
        {
            if (!body || !body->IsValid())
                continue;

            if (!m_Registry || !m_Registry->valid(entity))
                continue;

            if (!m_Registry->all_of<TransformComponent>(entity))
                continue;

            auto& transform = m_Registry->get<TransformComponent>(entity);
            body->SetPosition(transform.Translation);
            body->SetRotation(glm::quat(transform.Rotation));
        }
    }

    void PhysicsSystem::SyncTransformsFromPhysics()
    {
        for (auto& [entity, body] : m_EntityBodies)
        {
            if (!body || !body->IsValid())
                continue;

            // Skip kinematic and static bodies
            if (body->IsKinematic() || body->IsStatic())
                continue;

            if (!m_Registry || !m_Registry->valid(entity))
                continue;

            if (!m_Registry->all_of<TransformComponent>(entity))
                continue;

            auto& transform = m_Registry->get<TransformComponent>(entity);
            transform.Translation = body->GetPosition();
            glm::quat rotation = body->GetRotation();
            transform.Rotation = glm::eulerAngles(rotation);
        }
    }

    void PhysicsSystem::ProcessCollisions()
    {
        m_CollisionCallback.ProcessCollisions(PhysicsCore::Get().GetWorld()->GetDynamicsWorld());
    }

    PhysicsSystem::RaycastHit PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
    {
        RaycastHit hit;

        glm::vec3 end = origin + direction * maxDistance;
        auto result = PhysicsCore::Get().GetWorld()->Raycast(origin, end);

        if (result.Hit)
        {
            hit.Hit = true;
            hit.Point = result.HitPoint;
            hit.Normal = result.HitNormal;
            hit.Distance = glm::length(result.HitPoint - origin);

            if (result.HitBody)
            {
                void* userPointer = result.HitBody->getUserPointer();
                if (userPointer)
                {
                    hit.Entity = static_cast<entt::entity>(reinterpret_cast<intptr_t>(userPointer));
                }
            }
        }

        return hit;
    }

    void PhysicsSystem::EnableDebugDraw(bool enable)
    {
        m_DebugDrawEnabled = enable;
        PhysicsCore::Get().EnableDebugDraw(enable);
    }

    void PhysicsSystem::OnEntityDestroyed(entt::registry& registry, entt::entity entity)
    {
        DestroyPhysicsBody(entity);
    }

} // namespace Lunex
