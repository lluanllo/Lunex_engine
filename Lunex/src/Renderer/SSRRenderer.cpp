#include "stpch.h"
#include "SSRRenderer.h"
#include "RenderCommand.h"
#include "UniformBuffer.h"
#include "Framebuffer.h"
#include "VertexArray.h"
#include "Log/Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace Lunex {

	// ============================================================================
	// INTERNAL DATA
	// ============================================================================
	
	struct SSRRendererData {
		// Settings
		SSRSettings Settings;
		
		// Shaders
		Ref<Shader> SSRShader;
		Ref<Shader> CompositeShader;
		
		// Fullscreen quad VAO
		Ref<VertexArray> QuadVAO;
		Ref<VertexBuffer> QuadVBO;
		
		// Result framebuffer (abstracted)
		Ref<Framebuffer> ResultFramebuffer;
		uint32_t CurrentWidth = 0;
		uint32_t CurrentHeight = 0;
		
		// Uniform buffers
		Ref<UniformBuffer> SSRUniformBuffer;
		Ref<UniformBuffer> CompositeUniformBuffer;
		
		// SSR Uniform data structure (must match shader layout)
		struct SSRUniformData {
			glm::mat4 ViewMatrix;          // offset 0
			glm::mat4 ProjectionMatrix;    // offset 64
			glm::mat4 InvProjectionMatrix; // offset 128
			glm::mat4 InvViewMatrix;       // offset 192
			glm::vec2 ViewSize;            // offset 256
			float MaxDistance;             // offset 264
			float Resolution;              // offset 268
			float Thickness;               // offset 272
			float StepSize;                // offset 276
			int MaxSteps;                  // offset 280
			float Intensity;               // offset 284
			float RoughnessThreshold;      // offset 288
			float EdgeFade;                // offset 292
			int Enabled;                   // offset 296
			int UseEnvironmentFallback;    // offset 300
			float EnvironmentIntensity;    // offset 304
			float _padding;                // offset 308
		};
		
		// Composite Uniform data (binding = 7)
		struct CompositeUniformData {
			float Intensity;
			float _padding1;
			float _padding2;
			float _padding3;
		};
		
		SSRUniformData UniformData;
		CompositeUniformData CompositeData;
		
		bool Initialized = false;
	};
	
	static SSRRendererData s_Data;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================
	
	void SSRRenderer::Init() {
		if (s_Data.Initialized) return;
		
		LNX_LOG_INFO("Initializing SSR Renderer");
		
		// Load SSR shader
		s_Data.SSRShader = Shader::Create("assets/shaders/SSR.glsl");
		
		if (!s_Data.SSRShader) {
			LNX_LOG_ERROR("Failed to load SSR shader");
			return;
		}
		
		// Load Composite shader
		s_Data.CompositeShader = Shader::Create("assets/shaders/SSRComposite.glsl");
		
		if (!s_Data.CompositeShader) {
			LNX_LOG_ERROR("Failed to load SSR Composite shader");
			return;
		}
		
		// Create fullscreen quad
		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			
			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};
		
		s_Data.QuadVAO = VertexArray::Create();
		s_Data.QuadVBO = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		
		s_Data.QuadVBO->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});
		
		s_Data.QuadVAO->AddVertexBuffer(s_Data.QuadVBO);
		
		// Create uniform buffers (size matches new struct)
		s_Data.SSRUniformBuffer = UniformBuffer::Create(sizeof(SSRRendererData::SSRUniformData), 6);
		s_Data.CompositeUniformBuffer = UniformBuffer::Create(sizeof(SSRRendererData::CompositeUniformData), 7);
		
		// Initialize default settings
		s_Data.Settings = SSRSettings();
		
		s_Data.Initialized = true;
		
		LNX_LOG_INFO("SSR Renderer initialized successfully");
	}
	
	void SSRRenderer::Shutdown() {
		if (!s_Data.Initialized) return;
		
		LNX_LOG_INFO("Shutting down SSR Renderer");
		
		// Release abstracted resources
		s_Data.ResultFramebuffer.reset();
		s_Data.SSRShader.reset();
		s_Data.CompositeShader.reset();
		s_Data.QuadVAO.reset();
		s_Data.QuadVBO.reset();
		s_Data.SSRUniformBuffer.reset();
		s_Data.CompositeUniformBuffer.reset();
		
		s_Data.CurrentWidth = 0;
		s_Data.CurrentHeight = 0;
		s_Data.Initialized = false;
	}

	// ============================================================================
	// SETTINGS
	// ============================================================================
	
	void SSRRenderer::SetSettings(const SSRSettings& settings) {
		s_Data.Settings = settings;
	}
	
	SSRSettings& SSRRenderer::GetSettings() {
		return s_Data.Settings;
	}
	
	void SSRRenderer::SetEnabled(bool enabled) {
		s_Data.Settings.Enabled = enabled;
	}
	
	bool SSRRenderer::IsEnabled() {
		return s_Data.Settings.Enabled && s_Data.Initialized;
	}

	// ============================================================================
	// RESOURCE CREATION
	// ============================================================================
	
	void SSRRenderer::CreateResources() {
		// Create result framebuffer using abstraction
		FramebufferSpecification fbSpec;
		fbSpec.Width = s_Data.CurrentWidth;
		fbSpec.Height = s_Data.CurrentHeight;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA16F };  // HDR format for SSR
		fbSpec.Samples = 1;
		
		s_Data.ResultFramebuffer = Framebuffer::Create(fbSpec);
		
		LNX_LOG_TRACE("SSR framebuffer created: {0}x{1}", s_Data.CurrentWidth, s_Data.CurrentHeight);
	}
	
	void SSRRenderer::Resize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) return;
		
		// Ensure minimum size
		width = std::max(width, 1u);
		height = std::max(height, 1u);
		
		if (width == s_Data.CurrentWidth && height == s_Data.CurrentHeight) {
			return; // No resize needed
		}
		
		s_Data.CurrentWidth = width;
		s_Data.CurrentHeight = height;
		
		CreateResources();
	}

	// ============================================================================
	// UNIFORM BUFFER UPDATE
	// ============================================================================
	
	void SSRRenderer::UpdateUniformBuffer(
		const glm::mat4& viewMatrix,
		const glm::mat4& projectionMatrix,
		const glm::vec2& viewportSize
	) {
		s_Data.UniformData.ViewMatrix = viewMatrix;
		s_Data.UniformData.ProjectionMatrix = projectionMatrix;
		s_Data.UniformData.InvProjectionMatrix = glm::inverse(projectionMatrix);
		s_Data.UniformData.InvViewMatrix = glm::inverse(viewMatrix);
		s_Data.UniformData.ViewSize = viewportSize;
		s_Data.UniformData.MaxDistance = s_Data.Settings.MaxDistance;
		s_Data.UniformData.Resolution = s_Data.Settings.Resolution;
		s_Data.UniformData.Thickness = s_Data.Settings.Thickness;
		s_Data.UniformData.StepSize = s_Data.Settings.StepSize;
		s_Data.UniformData.MaxSteps = s_Data.Settings.MaxSteps;
		s_Data.UniformData.Intensity = s_Data.Settings.Intensity;
		s_Data.UniformData.RoughnessThreshold = s_Data.Settings.RoughnessThreshold;
		s_Data.UniformData.EdgeFade = s_Data.Settings.EdgeFade;
		s_Data.UniformData.Enabled = s_Data.Settings.Enabled ? 1 : 0;
		s_Data.UniformData.UseEnvironmentFallback = s_Data.Settings.UseEnvironmentFallback ? 1 : 0;
		s_Data.UniformData.EnvironmentIntensity = s_Data.Settings.EnvironmentIntensity;
		s_Data.UniformData._padding = 0.0f;
		
		s_Data.SSRUniformBuffer->SetData(&s_Data.UniformData, sizeof(SSRRendererData::SSRUniformData));
	}

	// ============================================================================
	// RENDER
	// ============================================================================
	
	void SSRRenderer::Render(
		uint32_t sceneColorTexture,
		uint32_t sceneDepthTexture,
		uint32_t sceneNormalTexture,
		uint32_t environmentMap,
		const glm::mat4& viewMatrix,
		const glm::mat4& projectionMatrix,
		const glm::vec2& viewportSize
	) {
		if (!s_Data.Initialized) {
			Init();
		}
		
		if (!s_Data.SSRShader) {
			return;
		}
		
		// Resize if needed
		uint32_t width = static_cast<uint32_t>(viewportSize.x);
		uint32_t height = static_cast<uint32_t>(viewportSize.y);
		
		if (width == 0 || height == 0) return;
		
		if (width != s_Data.CurrentWidth || height != s_Data.CurrentHeight) {
			s_Data.CurrentWidth = width;
			s_Data.CurrentHeight = height;
			CreateResources();
		}
		
		if (!s_Data.ResultFramebuffer) {
			return;
		}
		
		// Update uniform buffer
		UpdateUniformBuffer(viewMatrix, projectionMatrix, viewportSize);
		
		// Bind result framebuffer
		s_Data.ResultFramebuffer->Bind();
		RenderCommand::SetViewport(0, 0, s_Data.CurrentWidth, s_Data.CurrentHeight);
		RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::Clear();
		
		// Bind shader
		s_Data.SSRShader->Bind();
		
		// Bind textures to their respective binding points (matching shader layout)
		RenderCommand::BindTexture(0, sceneColorTexture);
		RenderCommand::BindTexture(1, sceneDepthTexture);
		RenderCommand::BindTexture(2, sceneNormalTexture);
		
		// Bind environment cubemap if available
		if (environmentMap != 0) {
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
		}
		
		// Disable depth testing for fullscreen pass
		RenderCommand::SetDepthMask(false);
		DepthFunc previousDepthFunc = RenderCommand::GetDepthFunc();
		RenderCommand::SetDepthFunc(DepthFunc::Always);
		
		// Draw fullscreen quad
		RenderCommand::DrawArrays(s_Data.QuadVAO, 6);
		
		// Restore state
		RenderCommand::SetDepthFunc(previousDepthFunc);
		RenderCommand::SetDepthMask(true);
		
		s_Data.ResultFramebuffer->Unbind();
	}
	
	// ============================================================================
	// COMPOSITE
	// ============================================================================
	
	void SSRRenderer::Composite(
		uint32_t sceneColorTexture,
		const glm::vec2& viewportSize
	) {
		if (!s_Data.Initialized || !s_Data.CompositeShader || !s_Data.ResultFramebuffer) {
			return;
		}
		
		// Update composite uniform buffer
		s_Data.CompositeData.Intensity = s_Data.Settings.Intensity;
		s_Data.CompositeData._padding1 = 0.0f;
		s_Data.CompositeData._padding2 = 0.0f;
		s_Data.CompositeData._padding3 = 0.0f;
		s_Data.CompositeUniformBuffer->SetData(&s_Data.CompositeData, sizeof(SSRRendererData::CompositeUniformData));
		
		// The caller should have the target framebuffer already bound
		
		// IMPORTANT: Only write to color attachment (0), not entity ID (1) or normals (2)
		// This prevents the fullscreen quad from overwriting entity picking data
		RenderCommand::SetDrawBuffers({ 0 });
		
		// Bind composite shader
		s_Data.CompositeShader->Bind();
		
		// Bind textures (using layout binding from shader)
		RenderCommand::BindTexture(0, sceneColorTexture);
		RenderCommand::BindTexture(1, s_Data.ResultFramebuffer->GetColorAttachmentRendererID(0));
		
		// Disable depth for fullscreen pass
		RenderCommand::SetDepthMask(false);
		DepthFunc previousDepthFunc = RenderCommand::GetDepthFunc();
		RenderCommand::SetDepthFunc(DepthFunc::Always);
		
		// Draw fullscreen quad
		RenderCommand::DrawArrays(s_Data.QuadVAO, 6);
		
		// Restore state
		RenderCommand::SetDepthFunc(previousDepthFunc);
		RenderCommand::SetDepthMask(true);
		
		// Restore all draw buffers (0, 1, 2)
		RenderCommand::SetDrawBuffers({ 0, 1, 2 });
	}
	
	uint32_t SSRRenderer::GetResultTexture() {
		if (s_Data.ResultFramebuffer) {
			return s_Data.ResultFramebuffer->GetColorAttachmentRendererID(0);
		}
		return 0;
	}

}
