#include "stpch.h"
#include "OutlineRenderer.h"

#include "RenderCommand.h"
#include "Buffer.h"
#include "Texture.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Lunex {
	struct OutlineRendererData {
		struct OutlineSettings {
			glm::vec4 OutlineColor;
			int SelectedEntityID;
			float OutlineThickness;
			glm::vec2 ViewportSize;
		};
		
		Ref<Shader> OutlineShader;
		Ref<VertexArray> QuadVertexArray;
		Ref<UniformBuffer> SettingsUniformBuffer;
		
		OutlineSettings Settings;
	};
	
	static OutlineRendererData s_Data;
	
	void OutlineRenderer::Init() {
		LNX_PROFILE_FUNCTION();
		
		// Cargar shader
		s_Data.OutlineShader = Shader::Create("assets/shaders/OutlinePostProcess.glsl");
		LNX_CORE_ASSERT(s_Data.OutlineShader, "Failed to load OutlinePostProcess shader!");
		
		// Crear uniform buffer (binding 3 para no colisionar con Camera=0, Transform=1, Material=2)
		s_Data.SettingsUniformBuffer = UniformBuffer::Create(sizeof(OutlineRendererData::OutlineSettings), 3);
		
		// Crear quad para post-procesamiento
		CreateQuad();
		
		LNX_LOG_INFO("OutlineRenderer initialized successfully");
	}
	
	void OutlineRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
		s_Data.OutlineShader.reset();
		s_Data.SettingsUniformBuffer.reset();
		s_Data.QuadVertexArray.reset();
	}
	
	void OutlineRenderer::CreateQuad() {
		if (s_Data.QuadVertexArray)
			return;
		
		// Quad de pantalla completa en NDC
		float quadVertices[] = {
			// Posiciones        // TexCoords
			-1.0f,  1.0f, 0.0f,0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,  1.0f, 1.0f
		};
		
		uint32_t quadIndices[] = { 0, 1, 2, 2, 3, 0 };
		
		s_Data.QuadVertexArray = VertexArray::Create();
		
		Ref<VertexBuffer> quadVB = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		quadVB->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoords" }
			});
		s_Data.QuadVertexArray->AddVertexBuffer(quadVB);
		
		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, 6);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIB);
	}
	
	void OutlineRenderer::RenderOutline(
		const Ref<Framebuffer>& mainFramebuffer,
		int selectedEntityID,
		const glm::vec4& outlineColor,
		float outlineThickness)
	{
		LNX_PROFILE_FUNCTION();
		
		if (selectedEntityID == -1 || !mainFramebuffer)
			return;
		
		auto spec = mainFramebuffer->GetSpecification();
		
		// Configurar settings
		s_Data.Settings.OutlineColor = outlineColor;
		s_Data.Settings.SelectedEntityID = selectedEntityID;
		s_Data.Settings.OutlineThickness = outlineThickness;
		s_Data.Settings.ViewportSize = glm::vec2(spec.Width, spec.Height);
		
		// Debug logging (solo primera vez)
		static bool firstRender = true;
		if (firstRender) {
			LNX_LOG_INFO("OutlineRenderer - First render:");
			LNX_LOG_INFO("  Entity ID: {0}", selectedEntityID);
			LNX_LOG_INFO("  Viewport Size: {0}x{1}", (int)spec.Width, (int)spec.Height);
			LNX_LOG_INFO("  Outline Color: ({0}, {1}, {2}, {3})", outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a);
			LNX_LOG_INFO("  Thickness: {0}", outlineThickness);
			firstRender = false;
		}
		
		// Actualizar uniform buffer
		s_Data.SettingsUniformBuffer->SetData(&s_Data.Settings, sizeof(OutlineRendererData::OutlineSettings));
		
		// Guardar estado actual de renderizado
		// (En una implementación real, deberías guardar TODOS los estados, pero por ahora solo los críticos)
		
		// Bind del shader de outline
		s_Data.OutlineShader->Bind();
		
		// Bind de la textura de Entity ID del framebuffer principal (attachment 1) al slot 0
		uint32_t entityTexID = mainFramebuffer->GetColorAttachmentRendererID(1);
		auto entityTexture = CreateRef<OpenGLTexture2D>(entityTexID, spec.Width, spec.Height, false);
		entityTexture->Bind(0);
		s_Data.OutlineShader->SetInt("u_EntityIDTexture", 0);
		
		// Configurar estados de renderizado usando RenderAPI
		RenderCommand::SetBlend(true);
		RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);
		RenderCommand::SetDepthTest(false);
		RenderCommand::SetDepthMask(false);
		
		// Dibujar quad de pantalla completa (dibuja outline directamente sobre la escena)
		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.QuadVertexArray);
		
		// Restaurar estados (NO desactivar blend, debe quedar activado)
		RenderCommand::SetDepthTest(true);
		RenderCommand::SetDepthMask(true);
	}
}