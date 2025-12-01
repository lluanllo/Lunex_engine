#include "stpch.h"
#include "PhysicsResourceManager.h"
#include "PhysicsUtils.h"
#include <sstream>
#include <iomanip>

namespace Lunex {

    PhysicsResourceManager& PhysicsResourceManager::Get()
    {
        static PhysicsResourceManager instance;
        return instance;
    }

    // ========================================
    // Primitive Shapes
    // ========================================

    btBoxShape* PhysicsResourceManager::GetBoxShape(const glm::vec3& halfExtents)
    {
        std::string key = GenerateBoxKey(halfExtents);
        
        auto it = m_PrimitiveShapes.find(key);
        if (it != m_PrimitiveShapes.end())
        {
            return static_cast<btBoxShape*>(it->second.get());
        }

        // Create new shape
        auto shape = std::make_unique<btBoxShape>(PhysicsUtils::ToBullet(halfExtents));
        btBoxShape* ptr = shape.get();
        m_PrimitiveShapes[key] = std::move(shape);
        return ptr;
    }

    btSphereShape* PhysicsResourceManager::GetSphereShape(float radius)
    {
        std::string key = GenerateSphereKey(radius);
        
        auto it = m_PrimitiveShapes.find(key);
        if (it != m_PrimitiveShapes.end())
        {
            return static_cast<btSphereShape*>(it->second.get());
        }

        // Create new shape
        auto shape = std::make_unique<btSphereShape>(radius);
        btSphereShape* ptr = shape.get();
        m_PrimitiveShapes[key] = std::move(shape);
        return ptr;
    }

    btCapsuleShape* PhysicsResourceManager::GetCapsuleShape(float radius, float height)
    {
        std::string key = GenerateCapsuleKey(radius, height);
        
        auto it = m_PrimitiveShapes.find(key);
        if (it != m_PrimitiveShapes.end())
        {
            return static_cast<btCapsuleShape*>(it->second.get());
        }

        // Create new shape
        auto shape = std::make_unique<btCapsuleShape>(radius, height);
        btCapsuleShape* ptr = shape.get();
        m_PrimitiveShapes[key] = std::move(shape);
        return ptr;
    }

    btCylinderShape* PhysicsResourceManager::GetCylinderShape(const glm::vec3& halfExtents)
    {
        std::string key = GenerateCylinderKey(halfExtents);
        
        auto it = m_PrimitiveShapes.find(key);
        if (it != m_PrimitiveShapes.end())
        {
            return static_cast<btCylinderShape*>(it->second.get());
        }

        // Create new shape
        auto shape = std::make_unique<btCylinderShape>(PhysicsUtils::ToBullet(halfExtents));
        btCylinderShape* ptr = shape.get();
        m_PrimitiveShapes[key] = std::move(shape);
        return ptr;
    }

    btConeShape* PhysicsResourceManager::GetConeShape(float radius, float height)
    {
        std::string key = GenerateConeKey(radius, height);
        
        auto it = m_PrimitiveShapes.find(key);
        if (it != m_PrimitiveShapes.end())
        {
            return static_cast<btConeShape*>(it->second.get());
        }

        // Create new shape
        auto shape = std::make_unique<btConeShape>(radius, height);
        btConeShape* ptr = shape.get();
        m_PrimitiveShapes[key] = std::move(shape);
        return ptr;
    }

    // ========================================
    // Named Shapes
    // ========================================

    btCollisionShape* PhysicsResourceManager::GetShape(const std::string& name)
    {
        auto it = m_Shapes.find(name);
        if (it != m_Shapes.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    void PhysicsResourceManager::RegisterShape(const std::string& name, std::unique_ptr<btCollisionShape> shape)
    {
        m_Shapes[name] = std::move(shape);
    }

    void PhysicsResourceManager::Clear()
    {
        m_Shapes.clear();
        m_PrimitiveShapes.clear();
    }

    void PhysicsResourceManager::RemoveShape(const std::string& name)
    {
        m_Shapes.erase(name);
    }

    // ========================================
    // Key Generation
    // ========================================

    std::string PhysicsResourceManager::GenerateBoxKey(const glm::vec3& halfExtents)
    {
        std::ostringstream oss;
        oss << "box_" << std::fixed << std::setprecision(4)
            << halfExtents.x << "_" << halfExtents.y << "_" << halfExtents.z;
        return oss.str();
    }

    std::string PhysicsResourceManager::GenerateSphereKey(float radius)
    {
        std::ostringstream oss;
        oss << "sphere_" << std::fixed << std::setprecision(4) << radius;
        return oss.str();
    }

    std::string PhysicsResourceManager::GenerateCapsuleKey(float radius, float height)
    {
        std::ostringstream oss;
        oss << "capsule_" << std::fixed << std::setprecision(4)
            << radius << "_" << height;
        return oss.str();
    }

    std::string PhysicsResourceManager::GenerateCylinderKey(const glm::vec3& halfExtents)
    {
        std::ostringstream oss;
        oss << "cylinder_" << std::fixed << std::setprecision(4)
            << halfExtents.x << "_" << halfExtents.y << "_" << halfExtents.z;
        return oss.str();
    }

    std::string PhysicsResourceManager::GenerateConeKey(float radius, float height)
    {
        std::ostringstream oss;
        oss << "cone_" << std::fixed << std::setprecision(4)
            << radius << "_" << height;
        return oss.str();
    }

} // namespace Lunex
