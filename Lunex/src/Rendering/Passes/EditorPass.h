#pragma once

/**
 * @file EditorPass.h
 * @brief Editor-specific rendering passes (grid, gizmos, selection outlines)
 */

#include "Rendering/RenderPass.h"

namespace Lunex {

	// ============================================================================
	// GRID PASS
	// ============================================================================
	
	/**
	 * @class GridPass
	 * @brief Renders editor grid on the ground plane
	 * 
	 * Features:
	 * - Infinite grid using shader
	 * - Fades with distance
	 * - Editor-only
	 */
	class GridPass : public RenderPassBase {
	public:
		GridPass() = default;
		virtual ~GridPass() = default;
		
		const char* GetName() const override { return "Grid Pass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
		// Configuration
		void SetGridSize(float size) { m_GridSize = size; }
		void SetGridColor(const glm::vec4& color) { m_GridColor = color; }
		
	private:
		void CreateGridResources();
		
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		// Grid resources
		Ref<RHI::RHIBuffer> m_GridVertexBuffer;
		Ref<RHI::RHIBuffer> m_GridIndexBuffer;
		Ref<RHI::RHIShader> m_GridShader;
		Ref<RHI::RHIGraphicsPipeline> m_GridPipeline;
		Ref<RHI::RHIBuffer> m_GridUniformBuffer;
		
		float m_GridSize = 100.0f;
		glm::vec4 m_GridColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
		bool m_ResourcesCreated = false;
	};

	// ============================================================================
	// GIZMO PASS
	// ============================================================================
	
	/**
	 * @class GizmoPass
	 * @brief Renders transformation gizmos (translate, rotate, scale)
	 * 
	 * Features:
	 * - Depth test disabled (always on top)
	 * - Color-coded axes (X=red, Y=green, Z=blue)
	 * - Interactive handles
	 */
	class GizmoPass : public RenderPassBase {
	public:
		GizmoPass() = default;
		virtual ~GizmoPass() = default;
		
		const char* GetName() const override { return "Gizmo Pass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		
		// Set selected entity for gizmo rendering
		void SetSelectedEntity(int entityID) { m_SelectedEntityID = entityID; }
		void SetGizmoTransform(const glm::mat4& transform) { m_GizmoTransform = transform; }
		
	private:
		RenderGraphResource m_ColorTarget;
		
		int m_SelectedEntityID = -1;
		glm::mat4 m_GizmoTransform = glm::mat4(1.0f);
		
		Ref<RHI::RHIShader> m_GizmoShader;
		Ref<RHI::RHIGraphicsPipeline> m_GizmoPipeline;
	};

	// ============================================================================
	// SELECTION OUTLINE PASS
	// ============================================================================
	
	/**
	 * @class SelectionOutlinePass
	 * @brief Renders outline around selected objects
	 * 
	 * Two-pass technique:
	 * 1. Render selected objects to stencil
	 * 2. Render slightly expanded version with outline color
	 */
	class SelectionOutlinePass : public RenderPassBase {
	public:
		SelectionOutlinePass() = default;
		virtual ~SelectionOutlinePass() = default;
		
		const char* GetName() const override { return "Selection Outline"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
		void SetSelectedEntity(int entityID) { m_SelectedEntityID = entityID; }
		void SetOutlineColor(const glm::vec4& color) { m_OutlineColor = color; }
		void SetOutlineWidth(float width) { m_OutlineWidth = width; }
		
	private:
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		int m_SelectedEntityID = -1;
		glm::vec4 m_OutlineColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
		float m_OutlineWidth = 2.0f;
		
		Ref<RHI::RHIShader> m_OutlineShader;
		Ref<RHI::RHIGraphicsPipeline> m_OutlinePipeline;
	};

	// ============================================================================
	// DEBUG VISUALIZATION PASS
	// ============================================================================
	
	/**
	 * @class DebugVisualizationPass
	 * @brief Renders debug visualizations (bounds, normals, etc.)
	 */
	class DebugVisualizationPass : public RenderPassBase {
	public:
		DebugVisualizationPass() = default;
		virtual ~DebugVisualizationPass() = default;
		
		const char* GetName() const override { return "Debug Visualization"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
		// Debug options
		void SetDrawBounds(bool draw) { m_DrawBounds = draw; }
		void SetDrawNormals(bool draw) { m_DrawNormals = draw; }
		void SetDrawWireframe(bool draw) { m_DrawWireframe = draw; }
		
	private:
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		bool m_DrawBounds = false;
		bool m_DrawNormals = false;
		bool m_DrawWireframe = false;
		
		Ref<RHI::RHIShader> m_DebugShader;
		Ref<RHI::RHIGraphicsPipeline> m_DebugPipeline;
	};

} // namespace Lunex
