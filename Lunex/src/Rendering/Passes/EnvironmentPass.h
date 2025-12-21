#pragma once

#include "Rendering/RenderPass.h"
#include <glm/glm.hpp>  // ? ADD THIS

namespace Lunex {

	// ============================================================================
	// SKYBOX PASS
	// ============================================================================
	
	/**
	 * @class SkyboxPass
	 * @brief Renders skybox/environment cubemap
	 * 
	 * Features:
	 * - Renders cubemap as background
	 * - Depth test = LessEqual (renders at far plane)
	 * - No depth write
	 * - Uses existing depth buffer
	 */
	class SkyboxPass : public RenderPassBase {
	public:
		SkyboxPass() = default;
		virtual ~SkyboxPass() = default;
		
		const char* GetName() const override { return "SkyboxPass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		void SetColorTarget(RenderGraphResource target) { m_ColorTarget = target; }
		void SetDepthTarget(RenderGraphResource target) { m_DepthTarget = target; }
		
	private:
		void CreateSkyboxResources();
		
		RenderGraphResource m_ColorTarget;
		RenderGraphResource m_DepthTarget;
		
		// Skybox resources
		Ref<RHI::RHIBuffer> m_SkyboxVertexBuffer;
		Ref<RHI::RHIBuffer> m_SkyboxIndexBuffer;
		Ref<RHI::RHIShader> m_SkyboxShader;
		Ref<RHI::RHIGraphicsPipeline> m_SkyboxPipeline;
		Ref<RHI::RHIBuffer> m_CameraUniformBuffer;
		
		bool m_ResourcesCreated = false;
	};

	// ============================================================================
	// IBL (IMAGE-BASED LIGHTING) PASS
	// ============================================================================
	
	/**
	 * @class IBLPass
	 * @brief Generates IBL data from environment map
	 * 
	 * This pass:
	 * - Generates irradiance map (diffuse)
	 * - Generates prefiltered specular map
	 * - Usually runs once when environment changes
	 */
	class IBLPass : public RenderPassBase {
	public:
		IBLPass() = default;
		virtual ~IBLPass() = default;
		
		const char* GetName() const override { return "IBL Pass"; }
		
		void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) override;
		void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) override;
		bool ShouldExecute(const SceneRenderInfo& sceneInfo) const override;
		
		// Set input environment cubemap
		void SetEnvironmentMap(RenderGraphResource envMap) { m_InputEnvironment = envMap; }
		
		// Get outputs
		RenderGraphResource GetIrradianceMap() const { return m_IrradianceMap; }
		RenderGraphResource GetPrefilteredMap() const { return m_PrefilteredMap; }
		
	private:
		RenderGraphResource m_InputEnvironment;
		RenderGraphResource m_IrradianceMap;
		RenderGraphResource m_PrefilteredMap;
		
		Ref<RHI::RHIShader> m_IrradianceShader;
		Ref<RHI::RHIShader> m_PrefilterShader;
		Ref<RHI::RHIComputePipeline> m_IrradiancePipeline;
		Ref<RHI::RHIComputePipeline> m_PrefilterPipeline;
		
		bool m_NeedsUpdate = true;
	};

} // namespace Lunex
