#pragma once

#include "../BulletPhysics.h"
#include <glm/glm.hpp>
#include <memory>
#include "../PhysicsMaterial.h"
#include "ColliderComponent.h"

namespace Lunex {

    // Forward declarations
    class CustomMotionState;
    class PhysicsWorld;

    /**
     * RigidBodyComponent - Wrapper around btRigidBody
     * 
     * Manages a Bullet rigid body with:
     * - Physics material properties
     * - Collision shape (from ColliderComponent)
     * - Forces and velocities
     * - Collision filtering
     */
    class RigidBodyComponent
    {
    public:
        RigidBodyComponent() = default;
        ~RigidBodyComponent();

        // Creation
        void Create(PhysicsWorld* world,
                   ColliderComponent* collider,
                   const PhysicsMaterial& material,
                   const glm::vec3& position,
                   const glm::quat& rotation);

        // Lifecycle
        void Destroy(PhysicsWorld* world);
        bool IsValid() const { return m_RigidBody != nullptr; }

        // Forces and impulses
        void ApplyForce(const glm::vec3& force, const glm::vec3& relativePos = glm::vec3(0.0f));
        void ApplyCentralForce(const glm::vec3& force);
        void ApplyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos = glm::vec3(0.0f));
        void ApplyCentralImpulse(const glm::vec3& impulse);
        void ApplyTorque(const glm::vec3& torque);
        void ApplyTorqueImpulse(const glm::vec3& torque);

        // Velocity
        void SetLinearVelocity(const glm::vec3& velocity);
        void SetAngularVelocity(const glm::vec3& velocity);
        glm::vec3 GetLinearVelocity() const;
        glm::vec3 GetAngularVelocity() const;

        // Transform
        void SetPosition(const glm::vec3& position);
        void SetRotation(const glm::quat& rotation);
        void SetTransform(const glm::vec3& position, const glm::quat& rotation);
        glm::vec3 GetPosition() const;
        glm::quat GetRotation() const;
        glm::mat4 GetTransformMatrix() const;

        // Properties
        void SetMass(float mass);
        float GetMass() const;
        void SetFriction(float friction);
        float GetFriction() const;
        void SetRestitution(float restitution);
        float GetRestitution() const;
        void SetDamping(float linear, float angular);

        // Constraints
        void SetLinearFactor(const glm::vec3& factor);  // Lock axes (e.g., 1,1,0 for 2D)
        void SetAngularFactor(const glm::vec3& factor); // Lock rotation axes

        // Kinematic/Static
        void SetKinematic(bool kinematic);
        bool IsKinematic() const;
        void SetStatic(bool isStatic);
        bool IsStatic() const;

        // Activation state
        void Activate();
        void SetActivationState(int state);
        bool IsActive() const;

        // Collision filtering
        void SetCollisionGroup(int group);
        void SetCollisionMask(int mask);

        // Material
        void SetMaterial(const PhysicsMaterial& material);
        const PhysicsMaterial& GetMaterial() const { return m_Material; }

        // CCD
        void EnableCCD(float motionThreshold, float sweptSphereRadius);
        void DisableCCD();

        // Raw access
        btRigidBody* GetRigidBody() const { return m_RigidBody.get(); }
        CustomMotionState* GetMotionState() const { return m_MotionState.get(); }

    private:
        void UpdateInertia();

    private:
        std::unique_ptr<btRigidBody> m_RigidBody;
        std::unique_ptr<CustomMotionState> m_MotionState;
        PhysicsMaterial m_Material;
        
        ColliderComponent* m_Collider = nullptr;
        int m_CollisionGroup = 1;
        int m_CollisionMask = -1; // All groups
    };

} // namespace Lunex
