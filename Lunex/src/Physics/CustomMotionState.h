#pragma once

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "PhysicsUtils.h"

namespace Lunex {

    // Forward declaration
    class Entity;

    /**
     * CustomMotionState - Synchronizes transform between Bullet and Lunex engine
     * 
     * Bullet calls:
     * - getWorldTransform() when creating the rigid body (to get initial transform)
     * - setWorldTransform() each simulation step (to update entity transform)
     */
    class CustomMotionState : public btMotionState
    {
    public:
        CustomMotionState(const glm::vec3& position, const glm::quat& rotation);
        virtual ~CustomMotionState() = default;

        // Called by Bullet to get the initial world transform
        void getWorldTransform(btTransform& worldTrans) const override;

        // Called by Bullet after simulation to update the entity transform
        void setWorldTransform(const btTransform& worldTrans) override;

        // Manual setters (for kinematic bodies or direct manipulation)
        void SetPosition(const glm::vec3& position);
        void SetRotation(const glm::quat& rotation);
        void SetTransform(const glm::vec3& position, const glm::quat& rotation);

        // Getters
        glm::vec3 GetPosition() const { return m_Position; }
        glm::quat GetRotation() const { return m_Rotation; }
        glm::mat4 GetTransformMatrix() const;

        // Entity association (optional - for future ECS integration)
        void SetEntity(void* entity) { m_Entity = entity; }
        void* GetEntity() const { return m_Entity; }

    private:
        glm::vec3 m_Position;
        glm::quat m_Rotation;
        void* m_Entity = nullptr; // Pointer to Entity (can be nullptr)
    };

} // namespace Lunex
