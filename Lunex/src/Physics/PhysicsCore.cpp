#include "stpch.h"
#include "PhysicsCore.h"

namespace Lunex {

    PhysicsCore& PhysicsCore::Get()
    {
        static PhysicsCore instance;
        return instance;
    }

    void PhysicsCore::Initialize(const PhysicsConfig& config)
    {
        if (m_Initialized)
        {
            // Already initialized, shutdown first
            Shutdown();
        }

        m_Config = config;
        m_World = std::make_unique<PhysicsWorld>(config);
        m_Accumulator = 0.0f;
        m_SimulationSteps = 0;
        m_Initialized = true;
    }

    void PhysicsCore::Shutdown()
    {
        if (!m_Initialized)
            return;

        m_World.reset();
        m_Accumulator = 0.0f;
        m_SimulationSteps = 0;
        m_Initialized = false;
    }

    void PhysicsCore::Update(float deltaTime)
    {
        if (!m_Initialized || !m_World)
            return;

        // Simple approach: just step with given deltaTime
        m_World->StepSimulation(deltaTime);
        m_SimulationSteps++;
    }

    void PhysicsCore::FixedUpdate(float deltaTime)
    {
        if (!m_Initialized || !m_World)
            return;

        // Fixed timestep with accumulator approach
        // This ensures deterministic physics simulation
        m_Accumulator += deltaTime;

        int steps = 0;
        while (m_Accumulator >= m_Config.FixedTimestep && steps < m_Config.MaxSubSteps)
        {
            m_World->StepSimulation(m_Config.FixedTimestep, 1, m_Config.FixedTimestep);
            m_Accumulator -= m_Config.FixedTimestep;
            m_SimulationSteps++;
            steps++;
        }

        // Clamp accumulator to prevent spiral of death
        if (m_Accumulator > m_Config.FixedTimestep * m_Config.MaxSubSteps)
        {
            m_Accumulator = m_Config.FixedTimestep;
        }
    }

    void PhysicsCore::SetConfig(const PhysicsConfig& config)
    {
        m_Config = config;
        
        if (m_World)
        {
            m_World->SetGravity(config.Gravity);
            // Note: Other config changes might require recreating the world
        }
    }

    void PhysicsCore::EnableDebugDraw(bool enable)
    {
        m_Config.EnableDebugDraw = enable;
    }

} // namespace Lunex
