#include "stpch.h"
#include "EnvironmentPass.h"
#include "Log/Log.h"
#include "Renderer/SkyboxRenderer.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	// Skybox cube vertices (center at origin, extends -1 to +1)
	static const float s_SkyboxVertices[] = {
		// Positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	// ============================================================================
	// SKYBOX PASS IMPLEMENTATION
	// ============================================================================
	
	void SkyboxPass::CreateSkyboxResources() {
		if (m_ResourcesCreated) return;
		
		// Resources will be created when pure RHI skybox is needed
		// For now, we use SkyboxRenderer which has its own resources
		
		m_ResourcesCreated = true;
	}
	
	void SkyboxPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Skybox Pass");
		
		// Read from depth (for depth test)
		builder.ReadTexture(m_DepthTarget);
		
		// Write to color
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void SkyboxPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		CreateSkyboxResources();
		
		// Use existing SkyboxRenderer which already uses RHI-compatible shaders
		auto environment = SkyboxRenderer::GetGlobalEnvironment();
		if (!environment || !environment->IsLoaded()) {
			return;
		}
		
		// Render skybox using existing renderer
		const auto& view = sceneInfo.View;
		SkyboxRenderer::Render(*environment, view.ViewMatrix, view.ProjectionMatrix);
	}
	
	bool SkyboxPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return SkyboxRenderer::IsEnabled() && SkyboxRenderer::HasEnvironmentLoaded();
	}

	// ============================================================================
	// IBL PASS IMPLEMENTATION
	// ============================================================================
	
	void IBLPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("IBL Generation");
		
		// Read input environment
		if (m_InputEnvironment.IsValid()) {
			builder.ReadTexture(m_InputEnvironment);
		}
		
		// Create output cubemaps if needed
		if (!m_IrradianceMap.IsValid()) {
			RenderGraphTextureDesc desc;
			desc.Width = 32;  // Small for diffuse irradiance
			desc.Height = 32;
			desc.Format = RHI::TextureFormat::RGBA16F;
			desc.DebugName = "IrradianceMap";
			m_IrradianceMap = builder.CreateTexture(desc);
		}
		
		if (!m_PrefilteredMap.IsValid()) {
			RenderGraphTextureDesc desc;
			desc.Width = 512;  // Larger for specular
			desc.Height = 512;
			desc.MipLevels = 5;  // Multiple mip levels for different roughness
			desc.Format = RHI::TextureFormat::RGBA16F;
			desc.DebugName = "PrefilteredEnvMap";
			m_PrefilteredMap = builder.CreateTexture(desc);
		}
		
		// Write to outputs
		builder.WriteTexture(m_IrradianceMap);
		builder.WriteTexture(m_PrefilteredMap);
	}
	
	void IBLPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!ShouldExecute(sceneInfo)) return;
		
		// IBL generation is already handled by EnvironmentMap class
		// when loading HDRI files. This pass is for future compute-based generation.
		
		// TODO: Implement compute shader-based IBL generation
		// For now, IBL is pre-computed when environment maps are loaded
		
		m_NeedsUpdate = false;
	}
	
	bool IBLPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return m_NeedsUpdate && m_InputEnvironment.IsValid();
	}

} // namespace Lunex
