#include "stpch.h"
#include "EditorPass.h"
#include "Log/Log.h"

namespace Lunex {

	// Grid quad vertices (fullscreen quad for infinite grid shader)
	static const float s_GridQuadVertices[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f
	};
	
	static const uint32_t s_GridQuadIndices[] = { 0, 1, 2, 2, 3, 0 };

	// ============================================================================
	// GRID PASS IMPLEMENTATION
	// ============================================================================
	
	void GridPass::CreateGridResources() {
		if (m_ResourcesCreated) return;
		
		// Create vertex/index buffers for quad
		// TODO: Create through RHIDevice when available
		
		// Create shader
		// m_GridShader = RHI::RHIShader::CreateFromFile("assets/shaders/Grid.glsl");
		
		// Create pipeline with alpha blending
		if (m_GridShader) {
			RHI::GraphicsPipelineDesc desc;
			desc.Shader = m_GridShader;
			desc.Blend = RHI::BlendState::AlphaBlend();
			desc.DepthStencil.DepthTestEnabled = true;
			desc.DepthStencil.DepthWriteEnabled = false;
			m_GridPipeline = RHI::RHIGraphicsPipeline::Create(desc);
		}
		
		m_ResourcesCreated = true;
	}
	
	void GridPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Grid Pass");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void GridPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		CreateGridResources();
		
		auto* cmdList = resources.GetCommandList();
		if (!cmdList || !m_GridPipeline) return;
		
		RHI::RenderPassBeginInfo passInfo;
		passInfo.Framebuffer = resources.GetRenderTarget().get();
		passInfo.ClearColor = false;
		passInfo.ClearDepth = false;
		
		cmdList->BeginRenderPass(passInfo);
		cmdList->SetPipeline(m_GridPipeline.get());
		
		// TODO: Render grid quad with infinite grid shader
		
		cmdList->EndRenderPass();
	}
	
	bool GridPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawGrid;
	}

	// ============================================================================
	// OTHER PASSES (SIMPLIFIED)
	// ============================================================================
	
	void GizmoPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Gizmo Pass");
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void GizmoPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		// TODO: Render gizmos
	}
	
	bool GizmoPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawGizmos && m_SelectedEntityID != -1;
	}
	
	void SelectionOutlinePass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Selection Outline");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void SelectionOutlinePass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		// TODO: Render selection outline
	}
	
	bool SelectionOutlinePass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return m_SelectedEntityID != -1;
	}
	
	void DebugVisualizationPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Debug Visualization");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void DebugVisualizationPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		// TODO: Render debug visualizations
	}
	
	bool DebugVisualizationPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawBounds || m_DrawBounds || m_DrawNormals || m_DrawWireframe;
	}

} // namespace Lunex
