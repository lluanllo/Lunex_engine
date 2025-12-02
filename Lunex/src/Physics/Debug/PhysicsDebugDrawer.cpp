#include "stpch.h"
#include "PhysicsDebugDrawer.h"
#include "../PhysicsUtils.h"
#include <iostream>

namespace Lunex {

    PhysicsDebugDrawer::PhysicsDebugDrawer()
        : m_DebugMode(0)
    {
        // Default: draw wireframe and contact points
        m_DebugMode = btIDebugDraw::DBG_DrawWireframe | 
                      btIDebugDraw::DBG_DrawContactPoints;
    }

    void PhysicsDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
    {
        glm::vec3 glmFrom = PhysicsUtils::ToGLM(from);
        glm::vec3 glmTo = PhysicsUtils::ToGLM(to);
        glm::vec3 glmColor = PhysicsUtils::ToGLM(color);

        // Store line for batch rendering
        m_Lines.push_back({ glmFrom, glmTo, glmColor });

        // Immediate callback (if set)
        if (m_DrawLineCallback)
        {
            m_DrawLineCallback(glmFrom, glmTo, glmColor);
        }
    }

    void PhysicsDebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB,
                                              btScalar distance, int lifeTime, const btVector3& color)
    {
        // Draw contact point as small cross
        btVector3 to = PointOnB + normalOnB * 0.1f;
        drawLine(PointOnB, to, color);

        // Draw perpendicular lines for better visualization
        btVector3 perpendicular(normalOnB.z(), normalOnB.x(), normalOnB.y());
        btVector3 p1 = PointOnB + perpendicular * 0.05f;
        btVector3 p2 = PointOnB - perpendicular * 0.05f;
        drawLine(p1, p2, color);
    }

    void PhysicsDebugDrawer::reportErrorWarning(const char* warningString)
    {
        std::cerr << "[Physics Warning] " << warningString << std::endl;
    }

    void PhysicsDebugDrawer::draw3dText(const btVector3& location, const char* textString)
    {
        // Text rendering not implemented - would require text rendering system
        // Could be implemented later with ImGui or custom text renderer
    }

    void PhysicsDebugDrawer::setDebugMode(int debugMode)
    {
        m_DebugMode = debugMode;
    }

    int PhysicsDebugDrawer::getDebugMode() const
    {
        return m_DebugMode;
    }

    void PhysicsDebugDrawer::Flush()
    {
        // Lines are stored in m_Lines buffer
        // User should retrieve them with GetLines() and render
        // Then call ClearLines() when done
    }

    void PhysicsDebugDrawer::DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color)
    {
        // Default implementation does nothing
        // Override this in subclass to connect to your renderer
        // Or use SetDrawLineCallback() to provide implementation
    }

} // namespace Lunex
