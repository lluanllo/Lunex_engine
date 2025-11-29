#pragma once

#include <glm/glm.hpp>

namespace Lunex {

    struct PhysicsConfig
    {
        // Gravity (meters/second^2)
        glm::vec3 Gravity = glm::vec3(0.0f, -9.81f, 0.0f);

        // Fixed timestep for physics simulation (seconds)
        // Recommended: 1/60 = 0.0166667f for 60 FPS physics
        float FixedTimestep = 1.0f / 60.0f;

        // Maximum number of sub-steps per frame
        // Higher values = more accurate but slower
        int MaxSubSteps = 10;

        // Solver iterations (higher = more stable but slower)
        int SolverIterations = 10;

        // Broadphase algorithm configuration
        glm::vec3 WorldAabbMin = glm::vec3(-1000.0f, -1000.0f, -1000.0f);
        glm::vec3 WorldAabbMax = glm::vec3(1000.0f, 1000.0f, 1000.0f);
        int MaxProxies = 32766;

        // CCD (Continuous Collision Detection) thresholds
        float DefaultCcdMotionThreshold = 0.0f;
        float DefaultCcdSweptSphereRadius = 0.0f;

        // Debug rendering
        bool EnableDebugDraw = false;
    };

} // namespace Lunex
