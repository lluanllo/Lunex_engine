#pragma once

/**
 * @file SceneRenderSystem.h
 * @brief Scene rendering system with dual-backend support
 * 
 * Supports switching between Rasterizer (existing Renderer3D) and
 * Path Tracer (compute-shader) at runtime.
 * 2D rendering (sprites, circles, gizmos, grid) is ALWAYS rasterized.
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Scene/Camera/EditorCamera.h"
#include "Rendering/RenderBackend.h"
#include "Rendering/Backends/RasterBackend.h"
#include "Rendering/Backends/RayTracingBackend.h"
#include "Core/Core.h"

namespace Lunex {

	class Camera;

	struct SceneRenderSettings {
		bool RenderGrid = true;
		bool RenderSkybox = true;
		bool RenderBillboards = true;
		bool RenderGizmos = true;
		bool RenderFrustums = true;
		float BillboardLineWidth = 0.15f;
	};

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
		bool IsActiveInMode(SceneMode mode) const override { return true; }

		// ========== Render API ==========
		void RenderScene(EditorCamera& camera);
		void RenderSceneRuntime(Camera& camera, const glm::mat4& cameraTransform);

		SceneRenderSettings& GetSettings() { return m_Settings; }
		const SceneRenderSettings& GetSettings() const { return m_Settings; }

		// ========== Backend API ==========
		void SetActiveBackend(RenderBackendType type);
		RenderBackendType GetActiveBackendType() const { return m_ActiveBackendType; }
		RenderBackend*    GetActiveBackend()           { return m_ActiveBackend; }
		const RenderBackend* GetActiveBackend() const  { return m_ActiveBackend; }

		/** Returns true when the system has a valid context and active backend. */
		bool IsReady() const { return m_Context != nullptr && m_ActiveBackend != nullptr; }

		/** Call when entities / materials change so RT backend rebuilds BVH. */
		void NotifySceneChanged();

		/** Call when a material property changes so the PT backend resets accumulation. */
		void NotifyMaterialChanged();

		/** Call when viewport resizes (needed by path tracer textures). */
		void OnViewportResize(uint32_t w, uint32_t h);

	private:
		// ========== Render Passes (2D, always raster) ==========
		void RenderSkybox(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderGrid(EditorCamera& camera);
		void RenderSprites(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderCircles(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderMeshes(const Camera& camera, const glm::mat4& cameraTransform);
		void RenderBillboards(const Camera& camera, const glm::vec3& cameraPosition);
		void RenderCameraFrustums(const Camera& camera);
		void RenderLightGizmos(const Camera& camera);
		glm::mat4 GetWorldTransform(entt::entity entity);

	private:
		static inline std::string s_Name = "SceneRenderSystem";

		SceneContext*       m_Context = nullptr;
		SceneRenderSettings m_Settings;

		// ========== Dual Backend ==========
		Scope<RasterBackend>      m_RasterBackend;
		Scope<RayTracingBackend>  m_RTBackend;
		RenderBackend*            m_ActiveBackend     = nullptr;
		RenderBackendType         m_ActiveBackendType = RenderBackendType::Rasterizer;
	};

} // namespace Lunex
