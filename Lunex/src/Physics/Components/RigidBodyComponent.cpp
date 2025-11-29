#include "stpch.h"
#include "RigidBodyComponent.h"
#include "../CustomMotionState.h"
#include "../PhysicsWorld.h"
#include "../PhysicsUtils.h"

namespace Lunex {

    RigidBodyComponent::~RigidBodyComponent()
    {
        // Note: Destroy should be called manually before destruction
        if (m_RigidBody)
        {
            // Warning: Not removed from world - ensure Destroy() is called
        }
    }

    void RigidBodyComponent::Create(PhysicsWorld* world,
                                     ColliderComponent* collider,
                                     const PhysicsMaterial& material,
                                     const glm::vec3& position,
                                     const glm::quat& rotation)
    {
        if (!world || !collider || !collider->IsValid())
            return;

        m_Material = material;
        m_Collider = collider;

        // Create motion state
        m_MotionState = std::make_unique<CustomMotionState>(position, rotation);

        // Calculate local inertia
        btVector3 localInertia(0, 0, 0);
        btCollisionShape* shape = collider->GetShape();

        if (material.Mass > 0.0f && !material.IsStatic && !material.IsKinematic)
        {
            shape->calculateLocalInertia(material.Mass, localInertia);
        }

        // Create rigid body
        btRigidBody::btRigidBodyConstructionInfo rbInfo(
            material.Mass,
            m_MotionState.get(),
            shape,
            localInertia
        );

        rbInfo.m_friction = material.Friction;
        rbInfo.m_restitution = material.Restitution;
        rbInfo.m_linearDamping = material.LinearDamping;
        rbInfo.m_angularDamping = material.AngularDamping;

        m_RigidBody = std::make_unique<btRigidBody>(rbInfo);

        // Set collision flags
        if (material.IsStatic)
        {
            m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        }
        else if (material.IsKinematic)
        {
            m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        }

        if (material.IsTrigger)
        {
            m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
        }

        // ========================================
        // ? IMPROVED CCD CONFIGURATION
        // ========================================
        
        // Enable CCD for dynamic objects by default
        if (!material.IsStatic && !material.IsKinematic)
        {
            if (material.UseCCD || material.Mass > 0.1f)
            {
                // Auto-enable CCD for dynamic objects to prevent tunneling
                float threshold = material.UseCCD ? material.CcdMotionThreshold : 0.5f;
                float radius = material.UseCCD ? material.CcdSweptSphereRadius : 0.2f;
                
                m_RigidBody->setCcdMotionThreshold(threshold);
                m_RigidBody->setCcdSweptSphereRadius(radius);
            }
        }
        
        // ? CRITICAL: Improve contact processing
        m_RigidBody->setContactProcessingThreshold(0.0f); // Process all contacts
        
        // ? CRITICAL: Better sleep threshold (prevent premature sleeping)
        m_RigidBody->setSleepingThresholds(0.5f, 0.5f); // Linear, Angular

        // Add to world
        world->AddRigidBody(m_RigidBody.get(), m_CollisionGroup, m_CollisionMask);
    }

    void RigidBodyComponent::Destroy(PhysicsWorld* world)
    {
        if (m_RigidBody && world)
        {
            world->RemoveRigidBody(m_RigidBody.get());
        }

        m_RigidBody.reset();
        m_MotionState.reset();
        m_Collider = nullptr;
    }

    // ========================================
    // Forces and Impulses
    // ========================================

    void RigidBodyComponent::ApplyForce(const glm::vec3& force, const glm::vec3& relativePos)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyForce(PhysicsUtils::ToBullet(force), PhysicsUtils::ToBullet(relativePos));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::ApplyCentralForce(const glm::vec3& force)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyCentralForce(PhysicsUtils::ToBullet(force));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::ApplyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyImpulse(PhysicsUtils::ToBullet(impulse), PhysicsUtils::ToBullet(relativePos));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::ApplyCentralImpulse(const glm::vec3& impulse)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyCentralImpulse(PhysicsUtils::ToBullet(impulse));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::ApplyTorque(const glm::vec3& torque)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyTorque(PhysicsUtils::ToBullet(torque));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::ApplyTorqueImpulse(const glm::vec3& torque)
    {
        if (m_RigidBody)
        {
            m_RigidBody->applyTorqueImpulse(PhysicsUtils::ToBullet(torque));
            m_RigidBody->activate();
        }
    }

    // ========================================
    // Velocity
    // ========================================

    void RigidBodyComponent::SetLinearVelocity(const glm::vec3& velocity)
    {
        if (m_RigidBody)
        {
            m_RigidBody->setLinearVelocity(PhysicsUtils::ToBullet(velocity));
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::SetAngularVelocity(const glm::vec3& velocity)
    {
        if (m_RigidBody)
        {
            m_RigidBody->setAngularVelocity(PhysicsUtils::ToBullet(velocity));
            m_RigidBody->activate();
        }
    }

    glm::vec3 RigidBodyComponent::GetLinearVelocity() const
    {
        if (m_RigidBody)
            return PhysicsUtils::ToGLM(m_RigidBody->getLinearVelocity());
        return glm::vec3(0.0f);
    }

    glm::vec3 RigidBodyComponent::GetAngularVelocity() const
    {
        if (m_RigidBody)
            return PhysicsUtils::ToGLM(m_RigidBody->getAngularVelocity());
        return glm::vec3(0.0f);
    }

    // ========================================
    // Transform
    // ========================================

    void RigidBodyComponent::SetPosition(const glm::vec3& position)
    {
        if (m_RigidBody && m_MotionState)
        {
            btTransform transform;
            m_MotionState->getWorldTransform(transform);
            transform.setOrigin(PhysicsUtils::ToBullet(position));
            m_RigidBody->setWorldTransform(transform);
            m_MotionState->setWorldTransform(transform);
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::SetRotation(const glm::quat& rotation)
    {
        if (m_RigidBody && m_MotionState)
        {
            btTransform transform;
            m_MotionState->getWorldTransform(transform);
            transform.setRotation(PhysicsUtils::ToBullet(rotation));
            m_RigidBody->setWorldTransform(transform);
            m_MotionState->setWorldTransform(transform);
            m_RigidBody->activate();
        }
    }

    void RigidBodyComponent::SetTransform(const glm::vec3& position, const glm::quat& rotation)
    {
        if (m_RigidBody && m_MotionState)
        {
            btTransform transform;
            transform.setOrigin(PhysicsUtils::ToBullet(position));
            transform.setRotation(PhysicsUtils::ToBullet(rotation));
            m_RigidBody->setWorldTransform(transform);
            m_MotionState->setWorldTransform(transform);
            m_RigidBody->activate();
        }
    }

    glm::vec3 RigidBodyComponent::GetPosition() const
    {
        if (m_MotionState)
            return m_MotionState->GetPosition();
        return glm::vec3(0.0f);
    }

    glm::quat RigidBodyComponent::GetRotation() const
    {
        if (m_MotionState)
            return m_MotionState->GetRotation();
        return glm::quat(1, 0, 0, 0);
    }

    glm::mat4 RigidBodyComponent::GetTransformMatrix() const
    {
        if (m_MotionState)
            return m_MotionState->GetTransformMatrix();
        return glm::mat4(1.0f);
    }

    // ========================================
    // Properties
    // ========================================

    void RigidBodyComponent::SetMass(float mass)
    {
        m_Material.Mass = mass;
        UpdateInertia();
    }

    float RigidBodyComponent::GetMass() const
    {
        if (m_RigidBody)
        {
            float invMass = m_RigidBody->getInvMass();
            return (invMass > 0.0f) ? (1.0f / invMass) : 0.0f;
        }
        return m_Material.Mass;
    }

    void RigidBodyComponent::SetFriction(float friction)
    {
        m_Material.Friction = friction;
        if (m_RigidBody)
            m_RigidBody->setFriction(friction);
    }

    float RigidBodyComponent::GetFriction() const
    {
        if (m_RigidBody)
            return m_RigidBody->getFriction();
        return m_Material.Friction;
    }

    void RigidBodyComponent::SetRestitution(float restitution)
    {
        m_Material.Restitution = restitution;
        if (m_RigidBody)
            m_RigidBody->setRestitution(restitution);
    }

    float RigidBodyComponent::GetRestitution() const
    {
        if (m_RigidBody)
            return m_RigidBody->getRestitution();
        return m_Material.Restitution;
    }

    void RigidBodyComponent::SetDamping(float linear, float angular)
    {
        m_Material.LinearDamping = linear;
        m_Material.AngularDamping = angular;
        if (m_RigidBody)
            m_RigidBody->setDamping(linear, angular);
    }

    void RigidBodyComponent::UpdateInertia()
    {
        if (!m_RigidBody || !m_Collider)
            return;

        btVector3 localInertia(0, 0, 0);
        if (m_Material.Mass > 0.0f)
        {
            m_Collider->GetShape()->calculateLocalInertia(m_Material.Mass, localInertia);
        }

        m_RigidBody->setMassProps(m_Material.Mass, localInertia);
        m_RigidBody->updateInertiaTensor();
    }

    // ========================================
    // Constraints
    // ========================================

    void RigidBodyComponent::SetLinearFactor(const glm::vec3& factor)
    {
        if (m_RigidBody)
            m_RigidBody->setLinearFactor(PhysicsUtils::ToBullet(factor));
    }

    void RigidBodyComponent::SetAngularFactor(const glm::vec3& factor)
    {
        if (m_RigidBody)
            m_RigidBody->setAngularFactor(PhysicsUtils::ToBullet(factor));
    }

    // ========================================
    // Kinematic/Static
    // ========================================

    void RigidBodyComponent::SetKinematic(bool kinematic)
    {
        m_Material.IsKinematic = kinematic;
        if (m_RigidBody)
        {
            if (kinematic)
            {
                m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                m_RigidBody->setActivationState(DISABLE_DEACTIVATION);
            }
            else
            {
                m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
            }
        }
    }

    bool RigidBodyComponent::IsKinematic() const
    {
        if (m_RigidBody)
            return m_RigidBody->isKinematicObject();
        return m_Material.IsKinematic;
    }

    void RigidBodyComponent::SetStatic(bool isStatic)
    {
        m_Material.IsStatic = isStatic;
        if (m_RigidBody)
        {
            if (isStatic)
            {
                m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            }
            else
            {
                m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
            }
        }
    }

    bool RigidBodyComponent::IsStatic() const
    {
        if (m_RigidBody)
            return m_RigidBody->isStaticObject();
        return m_Material.IsStatic;
    }

    // ========================================
    // Activation
    // ========================================

    void RigidBodyComponent::Activate()
    {
        if (m_RigidBody)
            m_RigidBody->activate(true);
    }

    void RigidBodyComponent::SetActivationState(int state)
    {
        if (m_RigidBody)
            m_RigidBody->setActivationState(state);
    }

    bool RigidBodyComponent::IsActive() const
    {
        if (m_RigidBody)
            return m_RigidBody->isActive();
        return false;
    }

    // ========================================
    // Collision Filtering
    // ========================================

    void RigidBodyComponent::SetCollisionGroup(int group)
    {
        m_CollisionGroup = group;
        // Note: Requires removing and re-adding to world to take effect
    }

    void RigidBodyComponent::SetCollisionMask(int mask)
    {
        m_CollisionMask = mask;
        // Note: Requires removing and re-adding to world to take effect
    }

    // ========================================
    // Material
    // ========================================

    void RigidBodyComponent::SetMaterial(const PhysicsMaterial& material)
    {
        m_Material = material;
        
        if (m_RigidBody)
        {
            SetMass(material.Mass);
            SetFriction(material.Friction);
            SetRestitution(material.Restitution);
            SetDamping(material.LinearDamping, material.AngularDamping);
            SetKinematic(material.IsKinematic);
            SetStatic(material.IsStatic);
            
            if (material.UseCCD)
                EnableCCD(material.CcdMotionThreshold, material.CcdSweptSphereRadius);
            else
                DisableCCD();
        }
    }

    // ========================================
    // CCD
    // ========================================

    void RigidBodyComponent::EnableCCD(float motionThreshold, float sweptSphereRadius)
    {
        m_Material.UseCCD = true;
        m_Material.CcdMotionThreshold = motionThreshold;
        m_Material.CcdSweptSphereRadius = sweptSphereRadius;

        if (m_RigidBody)
        {
            m_RigidBody->setCcdMotionThreshold(motionThreshold);
            m_RigidBody->setCcdSweptSphereRadius(sweptSphereRadius);
        }
    }

    void RigidBodyComponent::DisableCCD()
    {
        m_Material.UseCCD = false;
        m_Material.CcdMotionThreshold = 0.0f;
        m_Material.CcdSweptSphereRadius = 0.0f;

        if (m_RigidBody)
        {
            m_RigidBody->setCcdMotionThreshold(0.0f);
            m_RigidBody->setCcdSweptSphereRadius(0.0f);
        }
    }

} // namespace Lunex
