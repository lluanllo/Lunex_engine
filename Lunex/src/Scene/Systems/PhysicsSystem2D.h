#pragma once

/**
 * @file PhysicsSystem2D.h
 * @brief 2D Physics system using Box2D v3.x
 * 
 * AAA Architecture: PhysicsSystem2D lives in Scene/Systems/
 * Handles all 2D physics simulation for the scene.
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

#include "../Box2D/include/box2d/box2d.h"

namespace Lunex {

	/**
	 * @struct Physics2DSettings
	 * @brief Configuration for 2D physics simulation
	 */
	struct Physics2DSettings {
		glm::vec2 Gravity = { 0.0f, -9.8f };
		int VelocityIterations = 4;
		float FixedTimestep = 1.0f / 60.0f;
		bool EnableCCD = false;
	};

	/**
	 * @class PhysicsSystem2D
	 * @brief Scene system for 2D physics simulation
	 */
	class PhysicsSystem2D : public ISceneSystem {
	public:
		PhysicsSystem2D() = default;
		~PhysicsSystem2D() override;

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
		
		// ========== Physics2D Specific API ==========
		
		/**
		 * @brief Get physics settings
		 */
		const Physics2DSettings& GetSettings() const { return m_Settings; }
		
		/**
		 * @brief Update physics settings
		 */
		void SetSettings(const Physics2DSettings& settings);
		
		/**
		 * @brief Get the Box2D world (for advanced usage)
		 */
		b2WorldId GetWorld() const { return m_PhysicsWorld; }
		
		/**
		 * @brief Check if physics world is initialized
		 */
		bool IsWorldInitialized() const { return B2_IS_NON_NULL(m_PhysicsWorld); }
		
	private:
		// ========== Internal Methods ==========
		
		void CreatePhysicsWorld();
		void DestroyPhysicsWorld();
		
		void CreateRigidBodies();
		void SyncTransformsFromPhysics();
		void CleanupRuntimeBodies();
		
		// Box2D type conversion
		static b2BodyType ConvertBodyType(int type);
		
	private:
		static inline std::string s_Name = "PhysicsSystem2D";
		
		SceneContext* m_Context = nullptr;
		Physics2DSettings m_Settings;
		
		b2WorldId m_PhysicsWorld = B2_NULL_ID;
		
		// Accumulated time for fixed timestep
		float m_TimeAccumulator = 0.0f;
	};

} // namespace Lunex
