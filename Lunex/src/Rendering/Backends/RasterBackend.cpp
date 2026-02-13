#include "stpch.h"
#include "RasterBackend.h"
#include "Rendering/RenderSystem.h"
#include "Log/Log.h"

namespace Lunex {

	// ============================================================================
	// LIFECYCLE
	// ============================================================================

	void RasterBackend::Initialize(const RenderSystemConfig& config) {
		if (m_Initialized) return;

		LNX_LOG_INFO("[RasterBackend] Initializing...");

		m_GeometryPass           = CreateScope<GeometryPass>();
		m_TransparentPass        = CreateScope<TransparentPass>();
		m_SkyboxPass             = CreateScope<SkyboxPass>();
		m_IBLPass                = CreateScope<IBLPass>();
		m_GridPass               = CreateScope<GridPass>();
		m_GizmoPass              = CreateScope<GizmoPass>();
		m_SelectionOutlinePass   = CreateScope<SelectionOutlinePass>();
		m_DebugVisualizationPass = CreateScope<DebugVisualizationPass>();

		m_Initialized = true;
		LNX_LOG_INFO("[RasterBackend] Initialized with 8 passes");
	}

	void RasterBackend::Shutdown() {
		if (!m_Initialized) return;

		LNX_LOG_INFO("[RasterBackend] Shutting down...");

		m_GeometryPass.reset();
		m_TransparentPass.reset();
		m_SkyboxPass.reset();
		m_IBLPass.reset();
		m_GridPass.reset();
		m_GizmoPass.reset();
		m_SelectionOutlinePass.reset();
		m_DebugVisualizationPass.reset();

		m_Initialized = false;
	}

	void RasterBackend::OnViewportResize(uint32_t /*width*/, uint32_t /*height*/) {
		// RenderGraph manages resource sizes via ScaleToSwapchain — nothing to do
	}

	// ============================================================================
	// PER-FRAME
	// ============================================================================

	void RasterBackend::PrepareSceneData(const SceneRenderData& /*sceneData*/) {
		// Rasterization uploads data per-draw-call through Renderer3D.
		// Nothing additional to prepare here.
	}

	void RasterBackend::BuildRenderGraph(RenderGraph& graph, const SceneRenderInfo& sceneInfo) {
		if (!m_Initialized) return;

		BuildScenePasses(graph, sceneInfo);
		BuildEditorPasses(graph, sceneInfo);

		m_FinalColorTarget = m_GeometryPass->GetColorOutput();
		graph.SetBackbufferSource(m_FinalColorTarget);
	}

	// ============================================================================
	// SCENE PASSES  (identical logic to the original RenderSystem::BuildRenderGraph)
	// ============================================================================

	void RasterBackend::BuildScenePasses(RenderGraph& graph, const SceneRenderInfo& sceneInfo) {
		// ---- Geometry (opaque) ----
		graph.AddPass(
			"Geometry",
			[&](RenderPassBuilder& builder) { m_GeometryPass->Setup(graph, builder, sceneInfo); },
			[&](const RenderPassResources& resources) { m_GeometryPass->Execute(resources, sceneInfo); }
		);

		m_DepthTarget = m_GeometryPass->GetDepthOutput();

		// ---- Skybox ----
		if (m_SkyboxPass->ShouldExecute(sceneInfo)) {
			m_SkyboxPass->SetColorTarget(m_GeometryPass->GetColorOutput());
			m_SkyboxPass->SetDepthTarget(m_GeometryPass->GetDepthOutput());

			graph.AddPass(
				"Skybox",
				[&](RenderPassBuilder& builder) { m_SkyboxPass->Setup(graph, builder, sceneInfo); },
				[&](const RenderPassResources& resources) { m_SkyboxPass->Execute(resources, sceneInfo); }
			);
		}

		// ---- Transparent ----
		m_TransparentPass->SetColorTarget(m_GeometryPass->GetColorOutput());
		m_TransparentPass->SetDepthTarget(m_GeometryPass->GetDepthOutput());

		graph.AddPass(
			"Transparent",
			[&](RenderPassBuilder& builder) { m_TransparentPass->Setup(graph, builder, sceneInfo); },
			[&](const RenderPassResources& resources) { m_TransparentPass->Execute(resources, sceneInfo); }
		);
	}

	// ============================================================================
	// EDITOR PASSES
	// ============================================================================

	void RasterBackend::BuildEditorPasses(RenderGraph& graph, const SceneRenderInfo& sceneInfo) {
		// ---- Grid ----
		if (m_GridPass->ShouldExecute(sceneInfo)) {
			m_GridPass->SetColorTarget(m_GeometryPass->GetColorOutput());
			m_GridPass->SetDepthTarget(m_GeometryPass->GetDepthOutput());

			graph.AddPass(
				"Grid",
				[&](RenderPassBuilder& builder) { m_GridPass->Setup(graph, builder, sceneInfo); },
				[&](const RenderPassResources& resources) { m_GridPass->Execute(resources, sceneInfo); }
			);
		}

		// ---- Gizmos ----
		if (m_GizmoPass->ShouldExecute(sceneInfo)) {
			m_GizmoPass->SetColorTarget(m_GeometryPass->GetColorOutput());
			m_GizmoPass->SetSelectedEntity(m_SelectedEntityID);

			graph.AddPass(
				"Gizmos",
				[&](RenderPassBuilder& builder) { m_GizmoPass->Setup(graph, builder, sceneInfo); },
				[&](const RenderPassResources& resources) { m_GizmoPass->Execute(resources, sceneInfo); }
			);
		}

		// ---- Selection Outline ----
		if (m_SelectionOutlinePass->ShouldExecute(sceneInfo)) {
			m_SelectionOutlinePass->SetColorTarget(m_GeometryPass->GetColorOutput());
			m_SelectionOutlinePass->SetDepthTarget(m_GeometryPass->GetDepthOutput());
			m_SelectionOutlinePass->SetSelectedEntity(m_SelectedEntityID);

			graph.AddPass(
				"SelectionOutline",
				[&](RenderPassBuilder& builder) { m_SelectionOutlinePass->Setup(graph, builder, sceneInfo); },
				[&](const RenderPassResources& resources) { m_SelectionOutlinePass->Execute(resources, sceneInfo); }
			);
		}
	}

	// ============================================================================
	// CAPABILITIES
	// ============================================================================

	bool RasterBackend::SupportsFeature(const std::string& feature) const {
		if (feature == "ForwardRendering") return true;
		if (feature == "PBR")              return true;
		if (feature == "IBL")              return true;
		return false;
	}

} // namespace Lunex
