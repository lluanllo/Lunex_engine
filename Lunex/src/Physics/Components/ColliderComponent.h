#pragma once

#include "../BulletPhysics.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Lunex {

    /**
     * ColliderComponent - Defines collision shape for physics bodies
     * 
     * Supported shapes:
     * - Box
     * - Sphere
     * - Capsule
     * - Cylinder
     * - Cone
     * - Convex Hull (from mesh)
     * - Triangle Mesh (static only, concave)
     */
    class ColliderComponent
    {
    public:
        enum class ShapeType
        {
            Box,
            Sphere,
            Capsule,
            Cylinder,
            Cone,
            ConvexHull,
            TriangleMesh
        };

        ColliderComponent() = default;
        ~ColliderComponent();

        // Shape creation
        void CreateBoxShape(const glm::vec3& halfExtents);
        void CreateSphereShape(float radius);
        void CreateCapsuleShape(float radius, float height);
        void CreateCylinderShape(const glm::vec3& halfExtents);
        void CreateConeShape(float radius, float height);
        
        // Advanced shapes
        void CreateConvexHullShape(const std::vector<glm::vec3>& vertices);
        void CreateTriangleMeshShape(const std::vector<glm::vec3>& vertices, 
                                     const std::vector<uint32_t>& indices);

        // Compound shapes (multiple shapes combined)
        void CreateCompoundShape();
        void AddChildShape(btCollisionShape* shape, const glm::vec3& position, const glm::quat& rotation);

        // Getters
        btCollisionShape* GetShape() const { return m_Shape.get(); }
        ShapeType GetShapeType() const { return m_ShapeType; }
        
        // Offset (local position/rotation relative to body)
        void SetOffset(const glm::vec3& position, const glm::quat& rotation = glm::quat(1, 0, 0, 0));
        glm::vec3 GetOffsetPosition() const { return m_OffsetPosition; }
        glm::quat GetOffsetRotation() const { return m_OffsetRotation; }

        // Properties
        bool IsValid() const { return m_Shape != nullptr; }

    private:
        void Cleanup();

    private:
        std::unique_ptr<btCollisionShape> m_Shape;
        ShapeType m_ShapeType = ShapeType::Box;
        
        // For triangle mesh (needs to persist)
        std::unique_ptr<btTriangleIndexVertexArray> m_TriangleMesh;
        std::vector<btVector3> m_Vertices;
        std::vector<int> m_Indices;

        // Local offset
        glm::vec3 m_OffsetPosition = glm::vec3(0.0f);
        glm::quat m_OffsetRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

} // namespace Lunex
