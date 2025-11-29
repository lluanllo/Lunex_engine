#include "stpch.h"
#include "PhysicsSystem.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

namespace Lunex {

    PhysicsSystem::PhysicsSystem(Scene* scene)
        : m_Scene(scene)
    {
        // Get registry from scene (requires friend access or public getter)
        // For now, we'll assume Scene provides access to m_Registry
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
                // Extract entity IDs from user pointers
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

        // TODO: Create physics bodies for existing entities
        // This requires access to Scene's registry
        // m_Registry->view<TransformComponent, Rigidbody3DComponent>().each([this](auto entity, auto& transform, auto& rb) {
        //     CreatePhysicsBody(entity);
        // });

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

        // Shutdown physics core
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

        // Update physics world
        PhysicsCore::Get().Update(deltaTime);

        // Sync transforms from physics to entities
        SyncTransformsFromPhysics();

        // Process collisions
        ProcessCollisions();
    }

    void PhysicsSystem::OnFixedUpdate(float fixedDeltaTime)
    {
        if (!m_IsRunning)
            return;

        // Fixed timestep update
        PhysicsCore::Get().FixedUpdate(fixedDeltaTime);

        // Sync transforms from physics to entities
        SyncTransformsFromPhysics();

        // Process collisions
        ProcessCollisions();
    }

    void PhysicsSystem::CreatePhysicsBody(entt::entity entity)
    {
        // TODO: Implement body creation
        // This requires access to entity's components through registry
        // 1. Get TransformComponent
        // 2. Get Rigidbody3DComponent or create if not exists
        // 3. Get Collider3DComponent
        // 4. Create Bullet rigid body
        // 5. Add to world
        // 6. Store in m_EntityBodies
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
    }

    void PhysicsSystem::UpdatePhysicsBody(entt::entity entity)
    {
        // Recreate body with updated properties
        DestroyPhysicsBody(entity);
        CreatePhysicsBody(entity);
    }

    void PhysicsSystem::SyncTransformsToPhysics()
    {
        // Update Bullet bodies from TransformComponent
        // Useful for kinematic bodies
        for (auto& [entity, body] : m_EntityBodies)
        {
            if (!body || !body->IsValid())
                continue;

            // TODO: Get TransformComponent and sync to physics
            // auto& transform = m_Registry->get<TransformComponent>(entity);
            // body->SetPosition(transform.Translation);
            // body->SetRotation(glm::quat(transform.Rotation));
        }
    }

    void PhysicsSystem::SyncTransformsFromPhysics()
    {
        // Update TransformComponent from Bullet bodies
        // For dynamic bodies
        for (auto& [entity, body] : m_EntityBodies)
        {
            if (!body || !body->IsValid())
                continue;

            // Skip kinematic and static bodies
            if (body->IsKinematic() || body->IsStatic())
                continue;

            // TODO: Get TransformComponent and update from physics
            // auto& transform = m_Registry->get<TransformComponent>(entity);
            // transform.Translation = body->GetPosition();
            // glm::quat rotation = body->GetRotation();
            // transform.Rotation = glm::eulerAngles(rotation);
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

            // Extract entity ID from user pointer
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
