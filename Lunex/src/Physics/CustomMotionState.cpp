#include "stpch.h"
#include "CustomMotionState.h"
#include "PhysicsUtils.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

    CustomMotionState::CustomMotionState(const glm::vec3& position, const glm::quat& rotation)
        : m_Position(position), m_Rotation(rotation)
    {
    }

    void CustomMotionState::getWorldTransform(btTransform& worldTrans) const
    {
        worldTrans.setOrigin(PhysicsUtils::ToBullet(m_Position));
        worldTrans.setRotation(PhysicsUtils::ToBullet(m_Rotation));
    }

    void CustomMotionState::setWorldTransform(const btTransform& worldTrans)
    {
        m_Position = PhysicsUtils::GetPosition(worldTrans);
        m_Rotation = PhysicsUtils::GetRotation(worldTrans);

        // TODO: Update entity transform if m_Entity is valid
        // This is where you would call: entity->GetComponent<TransformComponent>().SetTransform(...)
    }

    void CustomMotionState::SetPosition(const glm::vec3& position)
    {
        m_Position = position;
    }

    void CustomMotionState::SetRotation(const glm::quat& rotation)
    {
        m_Rotation = rotation;
    }

    void CustomMotionState::SetTransform(const glm::vec3& position, const glm::quat& rotation)
    {
        m_Position = position;
        m_Rotation = rotation;
    }

    glm::mat4 CustomMotionState::GetTransformMatrix() const
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position);
        transform *= glm::mat4_cast(m_Rotation);
        return transform;
    }

} // namespace Lunex
