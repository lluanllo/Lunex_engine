#pragma once

/**
 * @file PhysicsSystem3D.h
 * @brief 3D Physics system using Bullet3
 * 
 * AAA Architecture: PhysicsSystem3D lives in Scene/Systems/
 * Handles all 3D physics simulation for the scene.
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

#include <glm/glm.hpp>

namespace Lunex {

	/**
	 * @struct Physics3DSettings
	 * @brief Configuration for 3D physics simulation
	 */
	struct Physics3DSettings {
		glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
		float FixedTimestep = 1.0f / 60.0f;
		int MaxSubsteps = 30;
		bool EnableCCD = false;
		float MaxVelocity = 50.0f;
	};

	/**
	 * @class PhysicsSystem3D
	 * @brief Scene system for 3D physics simulation using Bullet3
	 */
	class PhysicsSystem3D : public ISceneSystem {
	public:
		PhysicsSystem3D() = default;
		~PhysicsSystem3D() override;

		// ========== ISceneSystem Interface ==========
		
		void OnAttach(SceneContext& context) override;
		void OnDetach() override;
		
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		
		void OnUpdate(Timestep ts, SceneMode mode) override;
		void OnFixedUpdate(float fixedDeltaTime) override;
		
		void OnSceneEvent(const SceneSystemEvent& event) override;
		
		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Physics; }
		
		bool IsActiveInMode(SceneMode mode) const override {
			return IsPhysicsActiveInMode(mode);
		}
		
		// ========== Physics3D Specific API ==========
		
		/**
		 * @brief Get physics settings
		 */
		const Physics3DSettings& GetSettings() const { return m_Settings; }
		
		/**
		 * @brief Update physics settings
		 */
		void SetSettings(const Physics3DSettings& settings);
		
		/**
		 * @brief Check if physics is initialized
		 */
		bool IsInitialized() const { return m_Initialized; }
		
	private:
		// ========== Internal Methods ==========
		
		void InitializePhysics();
		void ShutdownPhysics();
		
		void CreateRigidBodies();
		void SyncTransformsFromPhysics();
		void ClampVelocities();
		void CleanupRuntimeBodies();
		
	private:
		static inline std::string s_Name = "PhysicsSystem3D";
		
		SceneContext* m_Context = nullptr;
		Physics3DSettings m_Settings;
		
		bool m_Initialized = false;
		float m_TimeAccumulator = 0.0f;
	};

} // namespace Lunex
