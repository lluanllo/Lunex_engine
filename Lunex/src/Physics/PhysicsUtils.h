#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <btBulletDynamicsCommon.h>

namespace Lunex {
namespace PhysicsUtils {

    // ========================================
    // GLM to Bullet conversions
    // ========================================

    inline btVector3 ToBullet(const glm::vec3& vec)
    {
        return btVector3(vec.x, vec.y, vec.z);
    }

    inline btQuaternion ToBullet(const glm::quat& quat)
    {
        return btQuaternion(quat.x, quat.y, quat.z, quat.w);
    }

    inline btTransform ToBullet(const glm::mat4& mat)
    {
        btTransform transform;
        transform.setFromOpenGLMatrix(&mat[0][0]);
        return transform;
    }

    // ========================================
    // Bullet to GLM conversions
    // ========================================

    inline glm::vec3 ToGLM(const btVector3& vec)
    {
        return glm::vec3(vec.x(), vec.y(), vec.z());
    }

    inline glm::quat ToGLM(const btQuaternion& quat)
    {
        return glm::quat(quat.w(), quat.x(), quat.y(), quat.z());
    }

    inline glm::mat4 ToGLM(const btTransform& transform)
    {
        glm::mat4 mat;
        transform.getOpenGLMatrix(&mat[0][0]);
        return mat;
    }

    // ========================================
    // Helper functions
    // ========================================

    inline glm::vec3 GetPosition(const btTransform& transform)
    {
        return ToGLM(transform.getOrigin());
    }

    inline glm::quat GetRotation(const btTransform& transform)
    {
        return ToGLM(transform.getRotation());
    }

    inline void SetPosition(btTransform& transform, const glm::vec3& position)
    {
        transform.setOrigin(ToBullet(position));
    }

    inline void SetRotation(btTransform& transform, const glm::quat& rotation)
    {
        transform.setRotation(ToBullet(rotation));
    }

    // Extract position and rotation from GLM mat4
    inline glm::vec3 ExtractPosition(const glm::mat4& mat)
    {
        return glm::vec3(mat[3]);
    }

    inline glm::quat ExtractRotation(const glm::mat4& mat)
    {
        return glm::quat_cast(mat);
    }

    inline glm::vec3 ExtractScale(const glm::mat4& mat)
    {
        return glm::vec3(
            glm::length(glm::vec3(mat[0])),
            glm::length(glm::vec3(mat[1])),
            glm::length(glm::vec3(mat[2]))
        );
    }

} // namespace PhysicsUtils
} // namespace Lunex
