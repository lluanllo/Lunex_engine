#include "stpch.h"
#include "ColliderComponent.h"
#include "../PhysicsWorld.h"
#include "../PhysicsUtils.h"

namespace Lunex {

    ColliderComponent::~ColliderComponent()
    {
        Cleanup();
    }

    void ColliderComponent::Cleanup()
    {
        m_Shape.reset();
        m_TriangleMesh.reset();
        m_Vertices.clear();
        m_Indices.clear();
    }

    void ColliderComponent::CreateBoxShape(const glm::vec3& halfExtents)
    {
        Cleanup();
        m_ShapeType = ShapeType::Box;
        m_Shape = std::make_unique<btBoxShape>(PhysicsUtils::ToBullet(halfExtents));
        
        // ? CRITICAL: Set proper collision margin
        m_Shape->setMargin(0.04f); // 4cm margin for stable collisions
    }

    void ColliderComponent::CreateSphereShape(float radius)
    {
        Cleanup();
        m_ShapeType = ShapeType::Sphere;
        m_Shape = std::make_unique<btSphereShape>(radius);
        
        // ? Spheres already have good margin by default, but set explicitly
        m_Shape->setMargin(0.04f);
    }

    void ColliderComponent::CreateCapsuleShape(float radius, float height)
    {
        Cleanup();
        m_ShapeType = ShapeType::Capsule;
        // btCapsuleShape: height is total height (including hemisphere caps)
        m_Shape = std::make_unique<btCapsuleShape>(radius, height);
        
        // ? Set margin
        m_Shape->setMargin(0.04f);
    }

    void ColliderComponent::CreateCylinderShape(const glm::vec3& halfExtents)
    {
        Cleanup();
        m_ShapeType = ShapeType::Cylinder;
        m_Shape = std::make_unique<btCylinderShape>(PhysicsUtils::ToBullet(halfExtents));
        
        // ? Set margin
        m_Shape->setMargin(0.04f);
    }

    void ColliderComponent::CreateConeShape(float radius, float height)
    {
        Cleanup();
        m_ShapeType = ShapeType::Cone;
        m_Shape = std::make_unique<btConeShape>(radius, height);
        
        // ? Set margin
        m_Shape->setMargin(0.04f);
    }

    void ColliderComponent::CreateConvexHullShape(const std::vector<glm::vec3>& vertices)
    {
        Cleanup();
        m_ShapeType = ShapeType::ConvexHull;

        auto convexHull = std::make_unique<btConvexHullShape>();
        
        for (const auto& vertex : vertices)
        {
            convexHull->addPoint(PhysicsUtils::ToBullet(vertex), false);
        }
        
        // Recalculate AABB after adding all points
        convexHull->recalcLocalAabb();
        
        // Optimize the hull
        convexHull->optimizeConvexHull();
        
        // ? Set margin for convex hull
        convexHull->setMargin(0.04f);
        
        m_Shape = std::move(convexHull);
    }

    void ColliderComponent::CreateTriangleMeshShape(const std::vector<glm::vec3>& vertices,
                                                     const std::vector<uint32_t>& indices)
    {
        Cleanup();
        m_ShapeType = ShapeType::TriangleMesh;

        // Convert to Bullet format
        m_Vertices.reserve(vertices.size());
        for (const auto& v : vertices)
        {
            m_Vertices.push_back(PhysicsUtils::ToBullet(v));
        }

        m_Indices.reserve(indices.size());
        for (auto idx : indices)
        {
            m_Indices.push_back(static_cast<int>(idx));
        }

        // Create triangle index vertex array
        int numTriangles = static_cast<int>(m_Indices.size() / 3);
        int numVertices = static_cast<int>(m_Vertices.size());

        m_TriangleMesh = std::make_unique<btTriangleIndexVertexArray>(
            numTriangles,
            m_Indices.data(),
            3 * sizeof(int),
            numVertices,
            reinterpret_cast<btScalar*>(m_Vertices.data()),
            sizeof(btVector3)
        );

        // Create BVH triangle mesh shape (static only!)
        bool useQuantizedAabbCompression = true;
        auto meshShape = std::make_unique<btBvhTriangleMeshShape>(
            m_TriangleMesh.get(),
            useQuantizedAabbCompression
        );
        
        // ? CRITICAL: Triangle meshes need margin too
        meshShape->setMargin(0.04f);
        
        m_Shape = std::move(meshShape);
    }

    void ColliderComponent::CreateCompoundShape()
    {
        Cleanup();
        m_ShapeType = ShapeType::Box; // Generic type for compound
        auto compound = std::make_unique<btCompoundShape>();
        
        // ? Set margin for compound
        compound->setMargin(0.04f);
        
        m_Shape = std::move(compound);
    }

    void ColliderComponent::AddChildShape(btCollisionShape* shape, const glm::vec3& position, const glm::quat& rotation)
    {
        if (!m_Shape || !shape)
            return;

        btCompoundShape* compound = dynamic_cast<btCompoundShape*>(m_Shape.get());
        if (!compound)
            return;

        btTransform localTransform;
        localTransform.setOrigin(PhysicsUtils::ToBullet(position));
        localTransform.setRotation(PhysicsUtils::ToBullet(rotation));

        compound->addChildShape(localTransform, shape);
    }

    void ColliderComponent::SetOffset(const glm::vec3& position, const glm::quat& rotation)
    {
        m_OffsetPosition = position;
        m_OffsetRotation = rotation;
    }

} // namespace Lunex
