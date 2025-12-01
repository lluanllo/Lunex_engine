#include "stpch.h"
#include "PhysicsWorld.h"
#include "PhysicsUtils.h"
#include "Debug/PhysicsDebugDrawer.h"

namespace Lunex {

    PhysicsWorld::PhysicsWorld(const PhysicsConfig& config)
        : m_Config(config)
    {
        Initialize(config);
    }

    PhysicsWorld::~PhysicsWorld()
    {
        Cleanup();
    }

    void PhysicsWorld::Initialize(const PhysicsConfig& config)
    {
        // 1. Collision configuration
        m_CollisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();

        // 2. Dispatcher
        m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfiguration.get());

        // 3. Broadphase
        // Using btDbvtBroadphase (Dynamic AABB tree) - good general purpose broadphase
        m_Broadphase = std::make_unique<btDbvtBroadphase>();

        // Alternative: btAxisSweep3 for limited world size (faster for smaller worlds)
        // m_Broadphase = std::make_unique<btAxisSweep3>(
        //     PhysicsUtils::ToBullet(config.WorldAabbMin),
        //     PhysicsUtils::ToBullet(config.WorldAabbMax),
        //     config.MaxProxies
        // );

        // 4. Constraint solver
        m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();

        // 5. Dynamics world
        m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
            m_Dispatcher.get(),
            m_Broadphase.get(),
            m_Solver.get(),
            m_CollisionConfiguration.get()
        );

        // ========================================
        // ? IMPROVED PHYSICS CONFIGURATION
        // ========================================
        
        // Configure gravity
        m_DynamicsWorld->setGravity(PhysicsUtils::ToBullet(config.Gravity));
        
        // Configure solver (more iterations = better stability)
        btContactSolverInfo& solverInfo = m_DynamicsWorld->getSolverInfo();
        solverInfo.m_numIterations = config.SolverIterations;
        solverInfo.m_solverMode = SOLVER_SIMD | SOLVER_USE_WARMSTARTING;
        
        // ? CRITICAL: Improve contact resolution (más agresivo para masas grandes)
        solverInfo.m_erp = 0.6f;  // ?? Error Reduction Parameter (0.6 = muy agresivo)
        solverInfo.m_erp2 = 0.6f; // ERP for split impulse
        solverInfo.m_globalCfm = 0.0f; // Constraint Force Mixing (0 = duro, sin softness)
        
        // ? CRITICAL: Enable split impulse for better penetration recovery
        solverInfo.m_splitImpulse = true;
        solverInfo.m_splitImpulsePenetrationThreshold = -0.05f; // ? de -0.04 a -0.05 (5cm threshold)
        solverInfo.m_splitImpulseTurnErp = 0.2f; // ? NEW: Turn correction in split impulse
        
        // ? CRITICAL: Linear and angular dampings
        solverInfo.m_linearSlop = 0.0f; // Zero allowed penetration
        solverInfo.m_warmstartingFactor = 0.85f; // Warm starting for faster convergence
        
        // ? NEW: Contact breaking threshold (cuándo se considera que el contacto terminó)
        solverInfo.m_restingContactRestitutionThreshold = 0.5f; // m/s
        
        // ? NEW: Más pasos de solver para mejor convergencia
        solverInfo.m_minimumSolverBatchSize = 128; // Batch size mínimo
        
        // ? NEW: Friction handling for heavy objects
        solverInfo.m_frictionERP = 0.5f; // Friction error reduction (default 0.2)
        solverInfo.m_frictionCFM = 0.0f; // No softness in friction
        
        // ? NEW: Maximum error per iteration (prevent explosions with heavy objects)
        solverInfo.m_maxGyroscopicForce = 100.0f; // Cap gyroscopic forces
    }

    void PhysicsWorld::Cleanup()
    {
        if (m_DynamicsWorld)
        {
            // Remove all rigid bodies
            for (int i = m_DynamicsWorld->getNumCollisionObjects() - 1; i >= 0; --i)
            {
                btCollisionObject* obj = m_DynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body)
                {
                    m_DynamicsWorld->removeRigidBody(body);
                }
            }
        }

        // Unique_ptr will automatically delete in reverse order
        m_DynamicsWorld.reset();
        m_Solver.reset();
        m_Broadphase.reset();
        m_Dispatcher.reset();
        m_CollisionConfiguration.reset();
    }

    void PhysicsWorld::StepSimulation(float deltaTime)
    {
        StepSimulation(deltaTime, m_Config.MaxSubSteps, m_Config.FixedTimestep);
    }

    void PhysicsWorld::StepSimulation(float deltaTime, int maxSubSteps, float fixedTimeStep)
    {
        if (m_DynamicsWorld)
        {
            m_DynamicsWorld->stepSimulation(deltaTime, maxSubSteps, fixedTimeStep);
        }
    }

    void PhysicsWorld::AddRigidBody(btRigidBody* body)
    {
        if (m_DynamicsWorld && body)
        {
            m_DynamicsWorld->addRigidBody(body);
        }
    }

    void PhysicsWorld::AddRigidBody(btRigidBody* body, int group, int mask)
    {
        if (m_DynamicsWorld && body)
        {
            m_DynamicsWorld->addRigidBody(body, group, mask);
        }
    }

    void PhysicsWorld::RemoveRigidBody(btRigidBody* body)
    {
        if (m_DynamicsWorld && body)
        {
            m_DynamicsWorld->removeRigidBody(body);
        }
    }

    void PhysicsWorld::SetGravity(const glm::vec3& gravity)
    {
        m_Config.Gravity = gravity;
        if (m_DynamicsWorld)
        {
            m_DynamicsWorld->setGravity(PhysicsUtils::ToBullet(gravity));
        }
    }

    glm::vec3 PhysicsWorld::GetGravity() const
    {
        if (m_DynamicsWorld)
        {
            return PhysicsUtils::ToGLM(m_DynamicsWorld->getGravity());
        }
        return m_Config.Gravity;
    }

    void PhysicsWorld::SetDebugDrawer(PhysicsDebugDrawer* debugDrawer)
    {
        if (m_DynamicsWorld)
        {
            m_DynamicsWorld->setDebugDrawer(debugDrawer);
        }
    }

    void PhysicsWorld::DebugDrawWorld()
    {
        if (m_DynamicsWorld && m_Config.EnableDebugDraw)
        {
            m_DynamicsWorld->debugDrawWorld();
        }
    }

    PhysicsWorld::RaycastResult PhysicsWorld::Raycast(const glm::vec3& from, const glm::vec3& to)
    {
        RaycastResult result;

        if (!m_DynamicsWorld)
            return result;

        btVector3 btFrom = PhysicsUtils::ToBullet(from);
        btVector3 btTo = PhysicsUtils::ToBullet(to);

        btCollisionWorld::ClosestRayResultCallback rayCallback(btFrom, btTo);
        m_DynamicsWorld->rayTest(btFrom, btTo, rayCallback);

        if (rayCallback.hasHit())
        {
            result.Hit = true;
            result.HitPoint = PhysicsUtils::ToGLM(rayCallback.m_hitPointWorld);
            result.HitNormal = PhysicsUtils::ToGLM(rayCallback.m_hitNormalWorld);
            result.HitBody = const_cast<btRigidBody*>(btRigidBody::upcast(rayCallback.m_collisionObject));
            result.HitFraction = rayCallback.m_closestHitFraction;
        }

        return result;
    }

    int PhysicsWorld::GetNumRigidBodies() const
    {
        return m_DynamicsWorld ? m_DynamicsWorld->getNumCollisionObjects() : 0;
    }

    int PhysicsWorld::GetNumManifolds() const
    {
        return m_DynamicsWorld ? m_DynamicsWorld->getDispatcher()->getNumManifolds() : 0;
    }

} // namespace Lunex
