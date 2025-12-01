#include "stpch.h"
#include "CollisionCallback.h"
#include "PhysicsUtils.h"

namespace Lunex {

    void CollisionCallback::ProcessCollisions(btDynamicsWorld* world)
    {
        if (!world)
            return;

        // Clear current frame collisions
        m_CurrentCollisions.clear();

        // Get all contact manifolds
        btDispatcher* dispatcher = world->getDispatcher();
        int numManifolds = dispatcher->getNumManifolds();

        for (int i = 0; i < numManifolds; ++i)
        {
            btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);

            const btCollisionObject* objA = manifold->getBody0();
            const btCollisionObject* objB = manifold->getBody1();

            btRigidBody* bodyA = const_cast<btRigidBody*>(btRigidBody::upcast(objA));
            btRigidBody* bodyB = const_cast<btRigidBody*>(btRigidBody::upcast(objB));

            if (!bodyA || !bodyB)
                continue;

            int numContacts = manifold->getNumContacts();
            if (numContacts == 0)
                continue;

            // Get first contact point for event data
            btManifoldPoint& point = manifold->getContactPoint(0);

            CollisionEvent event;
            event.BodyA = bodyA;
            event.BodyB = bodyB;
            event.ContactPoint = PhysicsUtils::ToGLM(point.getPositionWorldOnA());
            event.ContactNormal = PhysicsUtils::ToGLM(point.m_normalWorldOnB);
            event.Penetration = point.getDistance();
            event.UserDataA = objA->getUserPointer();
            event.UserDataB = objB->getUserPointer();

            CollisionPair pair{ bodyA, bodyB };

            // Store in current collisions
            m_CurrentCollisions[pair] = event;

            // Check if this is a new collision (Enter) or existing (Stay)
            auto prevIt = m_PreviousCollisions.find(pair);
            if (prevIt == m_PreviousCollisions.end())
            {
                // New collision - Enter
                if (m_OnCollisionEnter)
                    m_OnCollisionEnter(event);
            }
            else
            {
                // Existing collision - Stay
                if (m_OnCollisionStay)
                    m_OnCollisionStay(event);
            }
        }

        // Check for collisions that ended (Exit)
        if (m_OnCollisionExit)
        {
            for (const auto& [pair, event] : m_PreviousCollisions)
            {
                // If collision was in previous frame but not current, it exited
                if (m_CurrentCollisions.find(pair) == m_CurrentCollisions.end())
                {
                    m_OnCollisionExit(event);
                }
            }
        }

        // Swap current and previous
        m_PreviousCollisions = m_CurrentCollisions;
    }

    void CollisionCallback::ClearHistory()
    {
        m_PreviousCollisions.clear();
        m_CurrentCollisions.clear();
    }

    // ========================================
    // ContactListener
    // ========================================

    btScalar ContactListener::addSingleResult(btManifoldPoint& cp,
                                             const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
                                             const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
    {
        if (!m_Callback)
            return 0.0f;

        const btCollisionObject* objA = colObj0Wrap->getCollisionObject();
        const btCollisionObject* objB = colObj1Wrap->getCollisionObject();

        btRigidBody* bodyA = const_cast<btRigidBody*>(btRigidBody::upcast(objA));
        btRigidBody* bodyB = const_cast<btRigidBody*>(btRigidBody::upcast(objB));

        CollisionEvent event;
        event.BodyA = bodyA;
        event.BodyB = bodyB;
        event.ContactPoint = PhysicsUtils::ToGLM(cp.getPositionWorldOnA());
        event.ContactNormal = PhysicsUtils::ToGLM(cp.m_normalWorldOnB);
        event.Penetration = cp.getDistance();
        event.UserDataA = objA->getUserPointer();
        event.UserDataB = objB->getUserPointer();

        m_Callback(event);

        return 0.0f;
    }

} // namespace Lunex
