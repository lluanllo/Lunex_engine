#include "stpch.h"
#include "GridRenderer.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"
#include "Renderer/UniformBuffer.h"

namespace Lunex {

	struct GridRendererData {
		struct CameraData {
			glm::mat4 ViewProjection;
			glm::vec3 CameraPosition;
			float _padding;
		};
		
		struct GridSettingsData {
			glm::vec3 XAxisColor;
			float GridScale;
			glm::vec3 ZAxisColor;
			float MinorLineThickness;
			glm::vec3 GridColor;
			float MajorLineThickness;
			float FadeDistance;
			float _padding1, _padding2, _padding3;
		};
		
		Ref<Shader> GridShader;
		Ref<VertexArray> GridVAO;
		Ref<VertexBuffer> GridVBO;
		Ref<IndexBuffer> GridIBO;
		
		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> GridSettingsUniformBuffer;
		
		CameraData CameraBuffer;
		GridSettingsData GridSettingsBuffer;
	};

	static GridRendererData s_Data;
	GridRenderer::GridSettings GridRenderer::s_Settings;

	void GridRenderer::Init() {
		LNX_PROFILE_FUNCTION();

		// Crear un quad fullscreen simple
		// El grid se calcula completamente en el fragment shader
		float quadVertices[] = {
			-1.0f, -1.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f
		};

		uint32_t quadIndices[] = {
			0, 1, 2,
			2, 3, 0
		};

		s_Data.GridVAO = VertexArray::Create();

		s_Data.GridVBO = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		s_Data.GridVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Position" }
		});
		s_Data.GridVAO->AddVertexBuffer(s_Data.GridVBO);

		s_Data.GridIBO = IndexBuffer::Create(quadIndices, 6);
		s_Data.GridVAO->SetIndexBuffer(s_Data.GridIBO);

		// Crear uniform buffers
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(GridRendererData::CameraData), 0);
		s_Data.GridSettingsUniformBuffer = UniformBuffer::Create(sizeof(GridRendererData::GridSettingsData), 1);
		
		// Cargar el shader del grid
		s_Data.GridShader = Shader::Create("assets/shaders/InfiniteGrid.glsl");
	}

	void GridRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
	}

	void GridRenderer::DrawGrid(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();

		// Preparar camera uniform buffer
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(GridRendererData::CameraData));
		
		// Preparar grid settings uniform buffer
		s_Data.GridSettingsBuffer.XAxisColor = s_Settings.XAxisColor;
		s_Data.GridSettingsBuffer.ZAxisColor = s_Settings.ZAxisColor;
		s_Data.GridSettingsBuffer.GridColor = s_Settings.GridColor;
		s_Data.GridSettingsBuffer.GridScale = s_Settings.GridScale;
		s_Data.GridSettingsBuffer.MinorLineThickness = s_Settings.MinorLineThickness;
		s_Data.GridSettingsBuffer.MajorLineThickness = s_Settings.MajorLineThickness;
		s_Data.GridSettingsBuffer.FadeDistance = s_Settings.FadeDistance;
		s_Data.GridSettingsUniformBuffer->SetData(&s_Data.GridSettingsBuffer, sizeof(GridRendererData::GridSettingsData));

		// ? CRITICAL FIX: Disable writing to entity ID attachment (attachment 1)
		// Only write to color attachment 0 (RGBA8)
		RenderCommand::SetDrawBuffers({ 0 });

		// Bind shader
		s_Data.GridShader->Bind();

		// Deshabilitar escritura en depth buffer para que el grid esté siempre atrás
		RenderCommand::SetDepthMask(false);

		// Renderizar el quad
		s_Data.GridVAO->Bind();
		RenderCommand::DrawIndexed(s_Data.GridVAO);

		// Restaurar depth mask
		RenderCommand::SetDepthMask(true);
		
		// ? CRITICAL FIX: Re-enable writing to all attachments after grid rendering
		RenderCommand::SetDrawBuffers({ 0, 1 });
	}

}
