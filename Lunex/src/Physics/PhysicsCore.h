#pragma once

#include <memory>
#include "PhysicsWorld.h"
#include "PhysicsConfig.h"

namespace Lunex {

    /**
     * PhysicsCore - Singleton manager for the physics system
     * 
     * Responsibilities:
     * - Initialize/shutdown physics world
     * - Manage fixed timestep updates
     * - Provide access to physics world
     * - Handle debug rendering
     */
    class PhysicsCore
    {
    public:
        static PhysicsCore& Get();

        // Lifecycle
        void Initialize(const PhysicsConfig& config = PhysicsConfig());
        void Shutdown();

        // Update (should be called from engine main loop)
        void Update(float deltaTime);
        
        // Fixed timestep update with accumulator
        void FixedUpdate(float deltaTime);

        // Access
        PhysicsWorld* GetWorld() { return m_World.get(); }
        const PhysicsWorld* GetWorld() const { return m_World.get(); }

        // Configuration
        void SetConfig(const PhysicsConfig& config);
        const PhysicsConfig& GetConfig() const { return m_Config; }

        // Debug
        void EnableDebugDraw(bool enable);
        bool IsDebugDrawEnabled() const { return m_Config.EnableDebugDraw; }

        // Statistics
        float GetAccumulatedTime() const { return m_Accumulator; }
        int GetSimulationSteps() const { return m_SimulationSteps; }

    private:
        PhysicsCore() = default;
        ~PhysicsCore() = default;

        // Prevent copying
        PhysicsCore(const PhysicsCore&) = delete;
        PhysicsCore& operator=(const PhysicsCore&) = delete;

    private:
        std::unique_ptr<PhysicsWorld> m_World;
        PhysicsConfig m_Config;
        
        // Fixed timestep accumulator
        float m_Accumulator = 0.0f;
        int m_SimulationSteps = 0;

        bool m_Initialized = false;
    };

} // namespace Lunex
