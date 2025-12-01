#pragma once

#include "PhysicsCore.h"
#include "PhysicsWorld.h"
#include "Components/RigidBodyComponent.h"
#include "Components/ColliderComponent.h"
#include "CollisionCallback.h"

#include <entt.hpp>
#include <unordered_map>
#include <functional>

namespace Lunex {

    // Forward declaration
    class Entity;
    class Scene;

    /**
     * PhysicsSystem - Manages 3D physics simulation with Bullet3
     * 
     * Integrates with Lunex ECS:
     * - Automatically creates/destroys Bullet bodies for entities with physics components
     * - Synchronizes transforms between Bullet and TransformComponent
     * - Handles collision events
     * - Provides query and debug functionality
     * 
     * Usage:
     * 1. Create system: PhysicsSystem physicsSystem(scene);
     * 2. Update: physicsSystem.OnUpdate(deltaTime);
     * 3. Add physics to entity:
     *    entity.AddComponent<Rigidbody3DComponent>();
     *    entity.AddComponent<BoxCollider3DComponent>();
     */
    class PhysicsSystem
    {
    public:
        PhysicsSystem(Scene* scene);
        ~PhysicsSystem();

        // Lifecycle
        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnSimulationStart();
        void OnSimulationStop();

        // Update
        void OnUpdate(float deltaTime);
        void OnFixedUpdate(float fixedDeltaTime);

        // Entity management
        void CreatePhysicsBody(entt::entity entity);
        void DestroyPhysicsBody(entt::entity entity);
        void UpdatePhysicsBody(entt::entity entity);

        // Collision callbacks
        using CollisionEnterCallback = std::function<void(entt::entity, entt::entity)>;
        using CollisionExitCallback = std::function<void(entt::entity, entt::entity)>;
        
        void SetOnCollisionEnter(CollisionEnterCallback callback) { m_OnCollisionEnter = callback; }
        void SetOnCollisionExit(CollisionExitCallback callback) { m_OnCollisionExit = callback; }

        // Queries
        struct RaycastHit
        {
            bool Hit = false;
            entt::entity Entity = entt::null;
            glm::vec3 Point;
            glm::vec3 Normal;
            float Distance = 0.0f;
        };
        RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);

        // Debug
        void EnableDebugDraw(bool enable);
        bool IsDebugDrawEnabled() const { return m_DebugDrawEnabled; }

        // Access
        PhysicsWorld* GetWorld() { return PhysicsCore::Get().GetWorld(); }

    private:
        void SyncTransformsToPhysics();
        void SyncTransformsFromPhysics();
        void ProcessCollisions();
        
        void OnEntityDestroyed(entt::registry& registry, entt::entity entity);

    private:
        Scene* m_Scene = nullptr;
        entt::registry* m_Registry = nullptr;

        // Entity tracking
        std::unordered_map<entt::entity, RigidBodyComponent*> m_EntityBodies;

        // Collision handling
        CollisionCallback m_CollisionCallback;
        CollisionEnterCallback m_OnCollisionEnter;
        CollisionExitCallback m_OnCollisionExit;

        bool m_DebugDrawEnabled = false;
        bool m_IsRunning = false;
    };

} // namespace Lunex
