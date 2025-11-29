#pragma once

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Lunex {

    /**
     * CollisionEvent - Information about a collision
     */
    struct CollisionEvent
    {
        btRigidBody* BodyA = nullptr;
        btRigidBody* BodyB = nullptr;
        glm::vec3 ContactPoint;
        glm::vec3 ContactNormal;
        float Penetration = 0.0f;
        void* UserDataA = nullptr;
        void* UserDataB = nullptr;
    };

    /**
     * CollisionCallback - Manages collision event callbacks
     * 
     * Usage:
     * 1. Register callbacks for collision events
     * 2. Call ProcessCollisions() after physics step
     * 3. Receive enter/stay/exit events
     */
    class CollisionCallback
    {
    public:
        using CollisionCallbackFn = std::function<void(const CollisionEvent&)>;

        CollisionCallback() = default;
        ~CollisionCallback() = default;

        // Register callbacks
        void SetOnCollisionEnter(CollisionCallbackFn callback) { m_OnCollisionEnter = callback; }
        void SetOnCollisionStay(CollisionCallbackFn callback) { m_OnCollisionStay = callback; }
        void SetOnCollisionExit(CollisionCallbackFn callback) { m_OnCollisionExit = callback; }

        // Process collisions (call after physics step)
        void ProcessCollisions(btDynamicsWorld* world);

        // Clear collision history
        void ClearHistory();

    private:
        struct CollisionPair
        {
            btRigidBody* BodyA;
            btRigidBody* BodyB;

            bool operator==(const CollisionPair& other) const
            {
                return (BodyA == other.BodyA && BodyB == other.BodyB) ||
                       (BodyA == other.BodyB && BodyB == other.BodyA);
            }
        };

        struct CollisionPairHash
        {
            size_t operator()(const CollisionPair& pair) const
            {
                // Simple hash combining pointers
                size_t h1 = std::hash<void*>{}(pair.BodyA);
                size_t h2 = std::hash<void*>{}(pair.BodyB);
                return h1 ^ (h2 << 1);
            }
        };

        CollisionCallbackFn m_OnCollisionEnter;
        CollisionCallbackFn m_OnCollisionStay;
        CollisionCallbackFn m_OnCollisionExit;

        // Track active collisions to detect enter/exit
        std::unordered_map<CollisionPair, CollisionEvent, CollisionPairHash> m_PreviousCollisions;
        std::unordered_map<CollisionPair, CollisionEvent, CollisionPairHash> m_CurrentCollisions;
    };

    /**
     * ContactListener - Alternative: Bullet's contact callback interface
     * 
     * This can be used instead of CollisionCallback for more direct
     * integration with Bullet's collision system.
     */
    class ContactListener : public btCollisionWorld::ContactResultCallback
    {
    public:
        using ContactCallbackFn = std::function<void(const CollisionEvent&)>;

        ContactListener() = default;

        void SetCallback(ContactCallbackFn callback) { m_Callback = callback; }

        btScalar addSingleResult(btManifoldPoint& cp,
                                const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
                                const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override;

    private:
        ContactCallbackFn m_Callback;
    };

} // namespace Lunex
