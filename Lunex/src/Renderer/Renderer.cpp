#include "stpch.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Renderer/Renderer2D/Renderer2D.h"

namespace Lunex {
	Scope<Renderer::SceneData> Renderer::s_SceneData = CreateScope<Renderer::SceneData>();
	Scope<RenderGraph> Renderer::s_RenderGraph = nullptr;
	Ref<RenderContext> Renderer::s_Context = nullptr;
	
	void Renderer::Init() {
		LNX_PROFILE_FUNCTION();
		
		// Inicializamos el contexto global del render (creado en Application)
		s_Context = CreateRef<RenderContext>();
		s_RenderGraph = CreateScope<RenderGraph>();
		
		RenderCommand::Init();
		Renderer2D::Init();
		
		LNX_LOG_INFO("Renderer initialized with API: {0}", (int)GetAPI());
	}
	
	void Renderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
		
		Renderer2D::Shutdown();
		
		if (s_RenderGraph)
			s_RenderGraph->Shutdown();
		
		s_RenderGraph.reset();
		s_Context.reset();
	}
	
	void Renderer::OnWindowResize(uint32_t width, uint32_t height) {
		LNX_PROFILE_FUNCTION();
		RenderCommand::SetViewport(0, 0, width, height);
	}
	
	void Renderer::BeginScene(OrthographicCamera& camera) {
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
		s_SceneData->CameraPosition = camera.GetPosition();
	}
	
	void Renderer::EndScene() {
		// 🔹 Aquí podrías integrar el RenderGraph para ejecutar los RenderPass registrados
		if (s_RenderGraph)
			s_RenderGraph->Execute(*s_Context);
	}
	
	void Renderer::BeginFrame() {
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
	}
	
	void Renderer::EndFrame() {
	}
	
	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform) {
		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		shader->SetMat4("u_Transform", transform);
		
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}
