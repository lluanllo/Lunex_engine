#include "stpch.h"
#include "Renderer3D.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/UniformBuffer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Log/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lunex {
	struct Renderer3DData {
		struct CameraData {
			glm::mat4 ViewProjection;
		};

		struct TransformData {
			glm::mat4 Transform;
		};

		struct MaterialUniformData {
			glm::vec4 Color;
			float Metallic;
			float Roughness;
			float Specular;
			float EmissionIntensity;
			glm::vec3 EmissionColor;
			float _padding1;
			glm::vec3 ViewPos;
			int UseTexture;
		};

		struct LightsUniformData {
			int NumLights;
			float _padding1, _padding2, _padding3;
			Light::LightData Lights[16];
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialUniformData MaterialBuffer;
		LightsUniformData LightsBuffer;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;

		Ref<Shader> MeshShader;

		glm::vec3 CameraPosition = { 0.0f, 0.0f, 0.0f };

		Renderer3D::Statistics Stats;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init() {
		LNX_PROFILE_FUNCTION();

		s_Data.MeshShader = Shader::Create("assets/shaders/Mesh3D.glsl");

		// Create uniform buffers
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);
		s_Data.TransformUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::TransformData), 1);
		s_Data.MaterialUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::MaterialUniformData), 2);
		s_Data.LightsUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::LightsUniformData), 3);
		
		// Initialize with zero lights
		s_Data.LightsBuffer.NumLights = 0;
		s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(Renderer3DData::LightsUniformData));
	}

	void Renderer3D::Shutdown() {
		LNX_PROFILE_FUNCTION();
	}

	void Renderer3D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraPosition = glm::vec3(0.0f);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::EndScene() {
		LNX_PROFILE_FUNCTION();
	}

	void Renderer3D::UpdateLights(Scene* scene) {
		LNX_PROFILE_FUNCTION();
		
		s_Data.LightsBuffer.NumLights = 0;
		
		auto view = scene->GetAllEntitiesWith<TransformComponent, LightComponent>();
		int lightIndex = 0;
		
		for (auto entity : view) {
			if (lightIndex >= 16) break;
			
			Entity e = { entity, scene };
			auto& transform = e.GetComponent<TransformComponent>();
			auto& light = e.GetComponent<LightComponent>();
			
			glm::vec3 position = transform.Translation;
			glm::vec3 direction = glm::normalize(
				glm::rotate(glm::quat(transform.Rotation), glm::vec3(0.0f, 0.0f, -1.0f))
			);
			
			s_Data.LightsBuffer.Lights[lightIndex] = 
				light.LightInstance->GetLightData(position, direction);
			lightIndex++;
		}
		
		s_Data.LightsBuffer.NumLights = lightIndex;
		s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, 
			sizeof(Renderer3DData::LightsUniformData));
	}

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID) {
		if (!meshComponent.MeshModel)
			return;

		DrawModel(transform, meshComponent.MeshModel, meshComponent.Color, entityID);
	}

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, int entityID) {
		if (!meshComponent.MeshModel)
			return;

		DrawModel(transform, meshComponent.MeshModel, materialComponent, entityID);
	}

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();

		if (!model || model->GetMeshes().empty())
			return;

		const_cast<Model*>(model.get())->SetEntityID(entityID);

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Update Material uniform buffer with default values
		s_Data.MaterialBuffer.Color = color;
		s_Data.MaterialBuffer.Metallic = 0.0f;
		s_Data.MaterialBuffer.Roughness = 0.5f;
		s_Data.MaterialBuffer.Specular = 0.5f;
		s_Data.MaterialBuffer.EmissionIntensity = 0.0f;
		s_Data.MaterialBuffer.EmissionColor = glm::vec3(0.0f);
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

		// Check if model has textures
		bool hasTextures = false;
		for (const auto& mesh : model->GetMeshes()) {
			if (!mesh->GetTextures().empty()) {
				hasTextures = true;
				break;
			}
		}
		s_Data.MaterialBuffer.UseTexture = hasTextures ? 1 : 0;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

		// Draw the model
		s_Data.MeshShader->Bind();
		model->Draw(s_Data.MeshShader);

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)model->GetMeshes().size();

		for (const auto& mesh : model->GetMeshes()) {
			s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, int entityID) {
		LNX_PROFILE_FUNCTION();

		if (!model || model->GetMeshes().empty())
			return;

		const_cast<Model*>(model.get())->SetEntityID(entityID);

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Update Material uniform buffer with MaterialComponent values
		auto& material = materialComponent.MaterialInstance;
		s_Data.MaterialBuffer.Color = material->GetColor();
		s_Data.MaterialBuffer.Metallic = material->GetMetallic();
		s_Data.MaterialBuffer.Roughness = material->GetRoughness();
		s_Data.MaterialBuffer.Specular = material->GetSpecular();
		s_Data.MaterialBuffer.EmissionIntensity = material->GetEmissionIntensity();
		s_Data.MaterialBuffer.EmissionColor = material->GetEmissionColor();
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

		// Check if model has textures
		bool hasTextures = false;
		for (const auto& mesh : model->GetMeshes()) {
			if (!mesh->GetTextures().empty()) {
				hasTextures = true;
				break;
			}
		}
		s_Data.MaterialBuffer.UseTexture = hasTextures ? 1 : 0;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

		// Draw the model
		s_Data.MeshShader->Bind();
		model->Draw(s_Data.MeshShader);

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)model->GetMeshes().size();

		for (const auto& mesh : model->GetMeshes()) {
			s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	void Renderer3D::ResetStats() {
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}

	Renderer3D::Statistics Renderer3D::GetStats() {
		return s_Data.Stats;
	}
}