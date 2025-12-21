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
		// Initialize RHI (if not already initialized)
		// ========================================
		// NOTE: RHI may already be initialized by Application for window creation
		if (!RHI::IsInitialized()) {
			RHI::Initialize(RHI::GraphicsAPI::OpenGL, nullptr);
			LNX_LOG_INFO("RHI initialized");
		}
		
		RHI::InitializeRenderState();
		
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
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		}
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
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			uint32_t indexCount = vertexArray->GetIndexBuffer() ? vertexArray->GetIndexBuffer()->GetCount() : 0;
			cmdList->DrawIndexed(indexCount);
		}
	}
}