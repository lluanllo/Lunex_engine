#pragma once

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

    /**
     * PhysicsDebugDrawer - Implements btIDebugDraw for visualizing physics
     * 
     * Draws:
     * - Collision shapes (wireframe)
     * - Contact points
     * - AABBs
     * - Normals
     * - Constraints
     * 
     * Usage:
     * 1. Create instance
     * 2. Set debug mode
     * 3. Call world->setDebugDrawer(this)
     * 4. Call world->debugDrawWorld() in render loop
     * 5. Implement DrawLineImpl() to connect to your renderer
     */
    class PhysicsDebugDrawer : public btIDebugDraw
    {
    public:
        PhysicsDebugDrawer();
        virtual ~PhysicsDebugDrawer() = default;

        // btIDebugDraw interface
        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
        void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, 
                             btScalar distance, int lifeTime, const btVector3& color) override;
        void reportErrorWarning(const char* warningString) override;
        void draw3dText(const btVector3& location, const char* textString) override;
        void setDebugMode(int debugMode) override;
        int getDebugMode() const override;

        // Rendering
        void Flush(); // Call this to render all queued lines

        // Custom draw implementation (override this in subclass or set callback)
        using DrawLineCallback = std::function<void(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color)>;
        void SetDrawLineCallback(DrawLineCallback callback) { m_DrawLineCallback = callback; }

        // Line buffer access (for batch rendering)
        struct DebugLine
        {
            glm::vec3 From;
            glm::vec3 To;
            glm::vec3 Color;
        };
        const std::vector<DebugLine>& GetLines() const { return m_Lines; }
        void ClearLines() { m_Lines.clear(); }

    protected:
        virtual void DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);

    private:
        int m_DebugMode;
        std::vector<DebugLine> m_Lines;
        DrawLineCallback m_DrawLineCallback;
    };

} // namespace Lunex
