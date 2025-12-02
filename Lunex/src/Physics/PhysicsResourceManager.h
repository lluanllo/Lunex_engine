#pragma once

#include <btBulletDynamicsCommon.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

namespace Lunex {

    /**
     * PhysicsResourceManager - Cache and reuse collision shapes
     * 
     * Benefits:
     * - Avoid creating duplicate shapes
     * - Reduce memory usage
     * - Faster creation of rigid bodies with same shape
     * 
     * Usage:
     * auto shape = PhysicsResourceManager::Get().GetBoxShape(extents);
     * // Returns cached shape or creates new one
     */
    class PhysicsResourceManager
    {
    public:
        static PhysicsResourceManager& Get();

        // Primitive shapes (cached by key)
        btBoxShape* GetBoxShape(const glm::vec3& halfExtents);
        btSphereShape* GetSphereShape(float radius);
        btCapsuleShape* GetCapsuleShape(float radius, float height);
        btCylinderShape* GetCylinderShape(const glm::vec3& halfExtents);
        btConeShape* GetConeShape(float radius, float height);

        // Named shapes (for meshes, etc.)
        btCollisionShape* GetShape(const std::string& name);
        void RegisterShape(const std::string& name, std::unique_ptr<btCollisionShape> shape);

        // Clear cache
        void Clear();
        void RemoveShape(const std::string& name);

        // Statistics
        size_t GetShapeCount() const { return m_Shapes.size() + m_PrimitiveShapes.size(); }

    private:
        PhysicsResourceManager() = default;
        ~PhysicsResourceManager() = default;

        // Prevent copying
        PhysicsResourceManager(const PhysicsResourceManager&) = delete;
        PhysicsResourceManager& operator=(const PhysicsResourceManager&) = delete;

        // Generate keys for primitive shapes
        std::string GenerateBoxKey(const glm::vec3& halfExtents);
        std::string GenerateSphereKey(float radius);
        std::string GenerateCapsuleKey(float radius, float height);
        std::string GenerateCylinderKey(const glm::vec3& halfExtents);
        std::string GenerateConeKey(float radius, float height);

    private:
        // Named shapes
        std::unordered_map<std::string, std::unique_ptr<btCollisionShape>> m_Shapes;
        
        // Primitive shapes cache
        std::unordered_map<std::string, std::unique_ptr<btCollisionShape>> m_PrimitiveShapes;
    };

} // namespace Lunex
