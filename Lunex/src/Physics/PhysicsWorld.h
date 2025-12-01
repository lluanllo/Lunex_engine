#pragma once

#include "BulletPhysics.h"
#include <memory>
#include <vector>
#include "PhysicsConfig.h"

namespace Lunex {

    // Forward declarations
    class PhysicsDebugDrawer;

    /**
     * PhysicsWorld - Wrapper around btDiscreteDynamicsWorld
     * 
     * Manages the Bullet physics world and all its components:
     * - Collision configuration
     * - Dispatcher
     * - Broadphase
     * - Solver
     * - Dynamics world
     */
    class PhysicsWorld
    {
    public:
        PhysicsWorld(const PhysicsConfig& config = PhysicsConfig());
        ~PhysicsWorld();

        // Simulation
        void StepSimulation(float deltaTime);
        void StepSimulation(float deltaTime, int maxSubSteps, float fixedTimeStep);

        // Rigid body management
        void AddRigidBody(btRigidBody* body);
        void AddRigidBody(btRigidBody* body, int group, int mask);
        void RemoveRigidBody(btRigidBody* body);

        // World properties
        void SetGravity(const glm::vec3& gravity);
        glm::vec3 GetGravity() const;

        // Debug drawing
        void SetDebugDrawer(PhysicsDebugDrawer* debugDrawer);
        void DebugDrawWorld();

        // Raycasting
        struct RaycastResult
        {
            bool Hit = false;
            glm::vec3 HitPoint;
            glm::vec3 HitNormal;
            btRigidBody* HitBody = nullptr;
            float HitFraction = 0.0f;
        };
        RaycastResult Raycast(const glm::vec3& from, const glm::vec3& to);

        // Getters
        btDiscreteDynamicsWorld* GetDynamicsWorld() { return m_DynamicsWorld.get(); }
        const btDiscreteDynamicsWorld* GetDynamicsWorld() const { return m_DynamicsWorld.get(); }

        // Statistics
        int GetNumRigidBodies() const;
        int GetNumManifolds() const;

    private:
        void Initialize(const PhysicsConfig& config);
        void Cleanup();

    private:
        std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfiguration;
        std::unique_ptr<btCollisionDispatcher> m_Dispatcher;
        std::unique_ptr<btBroadphaseInterface> m_Broadphase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver;
        std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;

        PhysicsConfig m_Config;
    };

} // namespace Lunex
