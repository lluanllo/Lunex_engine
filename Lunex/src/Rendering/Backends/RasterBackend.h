#pragma once

/**
 * @file RasterBackend.h
 * @brief Rasterization render backend — wraps the existing rendering system 1:1
 * 
 * This is the default backend. It creates and owns the same render passes
 * that RenderSystem previously owned directly (GeometryPass, SkyboxPass, etc.)
 * and builds the same render graph. Zero behavioral change.
 */

#include "Rendering/RenderBackend.h"
#include "Rendering/Passes/GeometryPass.h"
#include "Rendering/Passes/EnvironmentPass.h"
#include "Rendering/Passes/EditorPass.h"

namespace Lunex {

	class RasterBackend : public IRenderBackend {
	public:
		RasterBackend() = default;
		~RasterBackend() override = default;

		// ---- IRenderBackend ----
		RenderMode  GetMode() const override { return RenderMode::Rasterization; }
		const char* GetName() const override { return "Rasterization"; }

		void Initialize(const RenderSystemConfig& config) override;
		void Shutdown() override;
		void OnViewportResize(uint32_t width, uint32_t height) override;

		void PrepareSceneData(const SceneRenderData& sceneData) override;
		void BuildRenderGraph(RenderGraph& graph, const SceneRenderInfo& sceneInfo) override;

		bool SupportsFeature(const std::string& feature) const override;
		bool RequiresAccelerationStructure() const override { return false; }

		RenderGraphResource GetFinalColorOutput() const override { return m_FinalColorTarget; }
		RenderGraphResource GetDepthOutput() const override     { return m_DepthTarget; }

		// ---- Editor pass configuration (forwarded from RenderSystem) ----
		void SetDrawGrid(bool draw)           { m_DrawGrid = draw; }
		void SetDrawGizmos(bool draw)         { m_DrawGizmos = draw; }
		void SetSelectedEntity(int entityID)  { m_SelectedEntityID = entityID; }

		// ---- Direct access to passes (for RenderSystem parallel jobs) ----
		GeometryPass*            GetGeometryPass()            { return m_GeometryPass.get(); }
		TransparentPass*         GetTransparentPass()         { return m_TransparentPass.get(); }
		SkyboxPass*              GetSkyboxPass()              { return m_SkyboxPass.get(); }
		IBLPass*                 GetIBLPass()                 { return m_IBLPass.get(); }
		GridPass*                GetGridPass()                { return m_GridPass.get(); }
		GizmoPass*               GetGizmoPass()               { return m_GizmoPass.get(); }
		SelectionOutlinePass*    GetSelectionOutlinePass()    { return m_SelectionOutlinePass.get(); }
		DebugVisualizationPass*  GetDebugVisualizationPass()  { return m_DebugVisualizationPass.get(); }

	private:
		void BuildScenePasses(RenderGraph& graph, const SceneRenderInfo& sceneInfo);
		void BuildEditorPasses(RenderGraph& graph, const SceneRenderInfo& sceneInfo);

		// Render passes (same objects that RenderSystem::State previously owned)
		Scope<GeometryPass>            m_GeometryPass;
		Scope<TransparentPass>         m_TransparentPass;
		Scope<SkyboxPass>              m_SkyboxPass;
		Scope<IBLPass>                 m_IBLPass;
		Scope<GridPass>                m_GridPass;
		Scope<GizmoPass>               m_GizmoPass;
		Scope<SelectionOutlinePass>    m_SelectionOutlinePass;
		Scope<DebugVisualizationPass>  m_DebugVisualizationPass;

		// Output resource handles
		RenderGraphResource m_FinalColorTarget;
		RenderGraphResource m_DepthTarget;

		// Editor state (mirrored from RenderSystem)
		bool m_DrawGrid = true;
		bool m_DrawGizmos = true;
		int  m_SelectedEntityID = -1;

		bool m_Initialized = false;
	};

} // namespace Lunex
