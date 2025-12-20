#include "stpch.h"
#include "EditorPass.h"
#include "Log/Log.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/Renderer2D.h"

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
		
		// Resources will be created when pure RHI grid shader is ready
		// For now, we use GridRenderer which has its own resources
		
		m_ResourcesCreated = true;
	}
	
	void GridPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Grid Pass");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void GridPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		// Use existing GridRenderer for now
		// This renders grid lines using Renderer2D
		// TODO: Migrate to pure RHI infinite grid shader
		
		// GridRenderer uses Renderer2D::DrawLine which is already RHI-compatible via shaders
	}
	
	bool GridPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawGrid && sceneInfo.View.IsEditorCamera;
	}

	// ============================================================================
	// GIZMO PASS IMPLEMENTATION
	// ============================================================================
	
	void GizmoPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Gizmo Pass");
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void GizmoPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		// Gizmos are rendered via ImGuizmo in ViewportPanel
		// Light/Camera gizmos use Renderer2D which is RHI-compatible
		// TODO: Add custom gizmo rendering using RHI directly
	}
	
	bool GizmoPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawGizmos && sceneInfo.View.IsEditorCamera && m_SelectedEntityID != -1;
	}
	
	// ============================================================================
	// SELECTION OUTLINE PASS IMPLEMENTATION
	// ============================================================================
	
	void SelectionOutlinePass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Selection Outline");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void SelectionOutlinePass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		// Selection outline is currently rendered via Renderer2D::DrawRect
		// TODO: Implement stencil-based outline using RHI
	}
	
	bool SelectionOutlinePass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.View.IsEditorCamera && m_SelectedEntityID != -1;
	}
	
	// ============================================================================
	// DEBUG VISUALIZATION PASS IMPLEMENTATION
	// ============================================================================
	
	void DebugVisualizationPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Debug Visualization");
		builder.ReadTexture(m_DepthTarget);
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void DebugVisualizationPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		// Debug visualization (colliders, bounds, normals) uses Renderer2D
		// TODO: Add debug visualizations using RHI directly
	}
	
	bool DebugVisualizationPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.DrawBounds || m_DrawBounds || m_DrawNormals || m_DrawWireframe;
	}

} // namespace Lunex
