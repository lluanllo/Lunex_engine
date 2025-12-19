#include "stpch.h"
#include "EnvironmentPass.h"
#include "Log/Log.h"
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
		
		// Create vertex buffer
		RHI::BufferDesc vbDesc;
		vbDesc.Type = RHI::BufferType::Vertex;
		vbDesc.Usage = RHI::BufferUsage::Static;
		vbDesc.Size = sizeof(s_SkyboxVertices);
		vbDesc.Stride = 3 * sizeof(float);
		
		// TODO: Create through RHIDevice
		// m_SkyboxVertexBuffer = RHI::RHIDevice::Get()->CreateVertexBuffer(
		//     s_SkyboxVertices, sizeof(s_SkyboxVertices), vbDesc.Stride, RHI::BufferUsage::Static
		// );
		
		// Create shader (would load from file)
		// m_SkyboxShader = RHI::RHIShader::CreateFromFile("assets/shaders/Skybox.glsl");
		
		// Create pipeline
		if (m_SkyboxShader) {
			RHI::GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.Shader = m_SkyboxShader;
			
			// Vertex layout
			RHI::VertexLayout layout({
				RHI::VertexAttribute("a_Position", RHI::DataType::Float3)
			});
			pipelineDesc.VertexLayout = layout;
			
			// Rasterizer state
			pipelineDesc.Rasterizer = RHI::RasterizerState::Default();
			pipelineDesc.Rasterizer.Culling = RHI::CullMode::Front;  // Cull front faces
			
			// Depth state
			pipelineDesc.DepthStencil.DepthTestEnabled = true;
			pipelineDesc.DepthStencil.DepthWriteEnabled = false;  // Don't write depth
			pipelineDesc.DepthStencil.DepthCompareFunc = RHI::CompareFunc::LessEqual;  // Draw at far plane
			
			// Blend state
			pipelineDesc.Blend = RHI::BlendState::Opaque();
			
			m_SkyboxPipeline = RHI::RHIGraphicsPipeline::Create(pipelineDesc);
		}
		
		// Create camera uniform buffer
		RHI::BufferDesc ubDesc;
		ubDesc.Type = RHI::BufferType::Uniform;
		ubDesc.Usage = RHI::BufferUsage::Dynamic;
		ubDesc.Size = sizeof(glm::mat4) * 2;  // View and Projection
		
		// TODO: Create through RHIDevice
		// m_CameraUniformBuffer = RHI::RHIDevice::Get()->CreateUniformBuffer(ubDesc.Size, ubDesc.Usage);
		
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
		if (!sceneInfo.Environment) {
			return;  // No environment to render
		}
		
		CreateSkyboxResources();
		
		if (!m_SkyboxPipeline || !m_SkyboxVertexBuffer) {
			LNX_LOG_WARN("SkyboxPass: Resources not ready");
			return;
		}
		
		auto* cmdList = resources.GetCommandList();
		if (!cmdList) return;
		
		// Begin render pass
		RHI::RenderPassBeginInfo passInfo;
		passInfo.Framebuffer = resources.GetRenderTarget().get();
		passInfo.ClearColor = false;  // Don't clear, render on top
		passInfo.ClearDepth = false;
		
		cmdList->BeginRenderPass(passInfo);
		
		// Bind pipeline
		cmdList->SetPipeline(m_SkyboxPipeline.get());
		
		// Update camera uniforms (remove translation for skybox)
		if (m_CameraUniformBuffer) {
			struct SkyboxUniforms {
				glm::mat4 View;
				glm::mat4 Projection;
			} uniforms;
			
			// Remove translation from view matrix
			glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(sceneInfo.View.ViewMatrix));
			uniforms.View = viewNoTranslation;
			uniforms.Projection = sceneInfo.View.ProjectionMatrix;
			
			m_CameraUniformBuffer->SetData(&uniforms, sizeof(SkyboxUniforms));
			cmdList->SetUniformBuffer(m_CameraUniformBuffer.get(), 0, RHI::ShaderStage::Vertex);
		}
		
		// Bind environment cubemap
		// TODO: Get cubemap texture from environment
		// cmdList->SetTexture(sceneInfo.Environment->GetCubemap().get(), 0);
		
		// Draw skybox cube
		cmdList->SetVertexBuffer(m_SkyboxVertexBuffer.get(), 0, 0);
		
		RHI::DrawArrayArgs drawArgs;
		drawArgs.VertexCount = 36;
		drawArgs.InstanceCount = 1;
		cmdList->Draw(drawArgs);
		
		cmdList->EndRenderPass();
	}
	
	bool SkyboxPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return sceneInfo.Environment != nullptr;
	}

	// ============================================================================
	// IBL PASS IMPLEMENTATION
	// ============================================================================
	
	void IBLPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("IBL Generation");
		
		// Read input environment
		builder.ReadTexture(m_InputEnvironment);
		
		// Create output cubemaps
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
		if (!m_NeedsUpdate) return;
		
		auto* cmdList = resources.GetCommandList();
		if (!cmdList) return;
		
		// This would use compute shaders to generate:
		// 1. Irradiance map (convolution of environment)
		// 2. Prefiltered map (importance sampling for different roughness levels)
		
		// TODO: Implement IBL generation using compute shaders
		// For now, this is a placeholder
		
		LNX_LOG_INFO("IBLPass: Generating IBL maps (placeholder)");
		
		m_NeedsUpdate = false;
	}
	
	bool IBLPass::ShouldExecute(const SceneRenderInfo& sceneInfo) const {
		return m_NeedsUpdate && m_InputEnvironment.IsValid();
	}

} // namespace Lunex
