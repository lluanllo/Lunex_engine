#include "stpch.h"
#include "Renderer3D.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/UniformBuffer.h"

namespace Lunex {

	struct Renderer3DData {
		struct CameraData {
			glm::mat4 ViewProjection;
		};

		struct TransformData {
			glm::mat4 Transform;
		};

		struct MaterialData {
			glm::vec4 Color;
			glm::vec3 LightPos;
			float padding1;
			glm::vec3 ViewPos;
			float padding2;
			glm::vec3 LightColor;
			int UseTexture;
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialData MaterialBuffer;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;

		Ref<Shader> MeshShader;

		glm::vec3 LightPosition = { 5.0f, 5.0f, 5.0f };
		glm::vec3 LightColor = { 1.0f, 1.0f, 1.0f };
		glm::vec3 CameraPosition = { 0.0f, 0.0f, 0.0f };

		Renderer3D::Statistics Stats;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init()
	{
		LNX_PROFILE_FUNCTION();

		s_Data.MeshShader = Shader::Create("assets/shaders/Mesh3D.glsl");

		// Create uniform buffers
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);
		s_Data.TransformUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::TransformData), 1);
		s_Data.MaterialUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::MaterialData), 2);
	}

	void Renderer3D::Shutdown()
	{
		LNX_PROFILE_FUNCTION();
	}

	void Renderer3D::BeginScene(const OrthographicCamera& camera)
	{
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraPosition = glm::vec3(0.0f);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const EditorCamera& camera)
	{
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::EndScene()
	{
		LNX_PROFILE_FUNCTION();
	}

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID)
	{
		if (!meshComponent.MeshModel)
			return;

		DrawModel(transform, meshComponent.MeshModel, meshComponent.Color, entityID);
	}

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color, int entityID)
	{
		LNX_PROFILE_FUNCTION();

		if (!model || model->GetMeshes().empty())
			return;

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Update Material uniform buffer
		s_Data.MaterialBuffer.Color = color;
		s_Data.MaterialBuffer.LightPos = s_Data.LightPosition;
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.MaterialBuffer.LightColor = s_Data.LightColor;

		// Check if model has textures
		bool hasTextures = false;
		for (const auto& mesh : model->GetMeshes()) {
			if (!mesh->GetTextures().empty()) {
				hasTextures = true;
				break;
			}
		}
		s_Data.MaterialBuffer.UseTexture = hasTextures ? 1 : 0;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialData));

		// Draw the model
		s_Data.MeshShader->Bind();
		model->Draw(s_Data.MeshShader);

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)model->GetMeshes().size();

		for (const auto& mesh : model->GetMeshes()) {
			s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	void Renderer3D::SetLightPosition(const glm::vec3& position)
	{
		s_Data.LightPosition = position;
	}

	void Renderer3D::SetLightColor(const glm::vec3& color)
	{
		s_Data.LightColor = color;
	}

	void Renderer3D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}

	Renderer3D::Statistics Renderer3D::GetStats()
	{
		return s_Data.Stats;
	}

}
