#pragma once

/**
 * @file GeometryPass.h
 * @brief Geometry rendering pass (opaque meshes, deferred/forward)
 */

#include "Rendering/RenderPass.h"
#include "Scene/Components.h"
#include <glm/glm.hpp> // ? ADD THIS

namespace Lunex {

	// ============================================================================
	// GEOMETRY PASS
	// ============================================================================
	
	/**
	 * @class GeometryPass
	 * @brief Renders opaque geometry (meshes) to GBuffer or forward
	 * 
	 * This pass:
	 * - Collects all mesh components from the scene
	 * - Builds draw lists
	 * - Sorts front-to-back for optimal depth testing
	 * - Renders to color + depth targets
	 */
	class GeometryPass : public RenderPassBase {
	public:
		GeometryPass() = default;
		virtual ~GeometryPass() = default;
		
		const char* GetName() const override { return "GeometryPass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		
		// Configuration
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
		RenderGraphResource GetColorOutput() const { return m_ColorTarget; }
		RenderGraphResource GetDepthOutput() const { return m_DepthTarget; }
		
	private:
		void CollectDrawCommands(const SceneRenderInfo& sceneInfo, DrawList& outDrawList);
		void SetupUniforms(RHI::RHICommandList* cmdList, const ViewInfo& view);
		
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		// Uniform buffers (will be created once and reused)
		Ref<RHI::RHIBuffer> m_CameraUniformBuffer;
		Ref<RHI::RHIBuffer> m_ObjectUniformBuffer;
	};

	// ============================================================================
	// TRANSPARENT PASS
	// ============================================================================
	
	/**
	 * @class TransparentPass
	 * @brief Renders transparent geometry with alpha blending
	 * 
	 * This pass:
	 * - Renders after opaque geometry
	 * - Sorts back-to-front
	 * - Enables alpha blending
	 */
	class TransparentPass : public RenderPassBase {
	public:
		TransparentPass() = default;
		virtual ~TransparentPass() = default;
		
		const char* GetName() const override { return "TransparentPass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
	private:
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		Ref<RHI::RHIBuffer> m_CameraUniformBuffer;
	};

} // namespace Lunex
