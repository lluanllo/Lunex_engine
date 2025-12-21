#pragma once

/**
 * @file SceneRenderSystem.h
 * @brief Scene rendering system
 * 
 * AAA Architecture: SceneRenderSystem lives in Scene/Systems/
 * Handles rendering of scene entities (2D, 3D, billboards, gizmos).
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Scene/Camera/EditorCamera.h"
#include "Core/Core.h"

namespace Lunex {

	// Forward declarations
	class Camera;

	/**
	 * @struct SceneRenderSettings
	 * @brief Configuration for scene rendering
	 */
	struct SceneRenderSettings {
		bool RenderGrid = true;
		bool RenderSkybox = true;
		bool RenderBillboards = true;
		bool RenderGizmos = true;
		bool RenderFrustums = true;
		float BillboardLineWidth = 0.15f;
	};

	/**
	 * @class SceneRenderSystem
	 * @brief Scene system for rendering
	 */
	class SceneRenderSystem : public ISceneSystem {
	public:
		SceneRenderSystem() = default;
		~SceneRenderSystem() override = default;

		// ========== ISceneSystem Interface ==========
		
		void OnAttach(SceneContext& context) override;
		void OnDetach() override;
		
		void OnUpdate(Timestep ts, SceneMode mode) override;
		
		void OnSceneEvent(const SceneSystemEvent& event) override;
		
		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Render; }
		
		// Rendering runs in all modes
		bool IsActiveInMode(SceneMode mode) const override { return true; }
		
		// ========== Render API ==========
		
		/**
		 * @brief Render scene with editor camera
		 */
		void RenderScene(EditorCamera& camera);
		
		/**
		 * @brief Render scene with runtime camera
		 */
		void RenderSceneRuntime(Camera& camera, const glm::mat4& cameraTransform);
		
		/**
		 * @brief Get render settings
		 */
		SceneRenderSettings& GetSettings() { return m_Settings; }
		const SceneRenderSettings& GetSettings() const { return m_Settings; }
		
	private:
		// ========== Render Passes ==========
		
		void RenderSkybox(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderGrid(EditorCamera& camera);
		
		void RenderSprites(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderCircles(const Camera& camera, const glm::mat4& cameraTransform);
		
		void RenderMeshes(const Camera& camera, const glm::mat4& cameraTransform);
		
		void RenderBillboards(const Camera& camera, const glm::vec3& cameraPosition);
		void RenderCameraFrustums(const Camera& camera);
		void RenderLightGizmos(const Camera& camera);
		
		// Helpers
		glm::mat4 GetWorldTransform(entt::entity entity);
		
	private:
		static inline std::string s_Name = "SceneRenderSystem";
		
		SceneContext* m_Context = nullptr;
		SceneRenderSettings m_Settings;
	};

} // namespace Lunex
