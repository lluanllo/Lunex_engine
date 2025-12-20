#include "stpch.h"

#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "SkyboxRenderer.h"
#include "TextureCompression.h"
#include "RHI/RHI.h"

namespace Lunex {
	
	Scope<Renderer::SceneData> Renderer::s_SceneData = CreateScope<Renderer::SceneData>();
	
	void Renderer::Init() {
		LNX_PROFILE_FUNCTION();
		
		// ========================================
		// Initialize RHI (must be first)
		// ========================================
		RHI::Initialize(RHI::GraphicsAPI::OpenGL, nullptr);
		LNX_LOG_INFO("RHI initialized");
		
		RenderCommand::Init();
		Renderer2D::Init();
		Renderer3D::Init();
		SkyboxRenderer::Init();
		
		// Initialize texture compression system
		TextureCompressor::Get().Initialize();
		
		if (TextureCompressionConfig::IsKTXAvailable()) {
			LNX_LOG_INFO("Texture Compression: KTX enabled - textures will be compressed automatically");
		}
		else {
			LNX_LOG_WARN("Texture Compression: KTX not available - using uncompressed textures");
		}
	}
	
	void Renderer::Shutdown() {
		Renderer2D::Shutdown();
		Renderer3D::Shutdown();
		SkyboxRenderer::Shutdown();
		TextureCompressor::Get().Shutdown();
		
		// ========================================
		// Shutdown RHI (must be last)
		// ========================================
		RHI::Shutdown();
		LNX_LOG_INFO("RHI shutdown");
	}
	
	void Renderer::onWindowResize(uint32_t width, uint32_t height) {
		RenderCommand::SetViewport(0, 0, width, height);
	}
	
	void Renderer::BeginScene(OrthographicCamera& camera){
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}
	
	void Renderer::EndScene() {
	
	}
	
	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform) {
		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		shader->SetMat4("u_Transform", transform);
		
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}