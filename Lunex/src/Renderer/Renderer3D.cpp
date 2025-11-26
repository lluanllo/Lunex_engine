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
			glm::mat4 View;
			glm::mat4 Projection;
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

			// Texture flags
			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			float _padding2;

			// Texture multipliers
			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;
		};

		struct LightsUniformData {
			int NumLights;
			float _padding1, _padding2, _padding3;
			Light::LightData Lights[16];
		};
		
		struct SkyboxLightingData {
			glm::vec3 SkyboxAmbient;
			float SkyboxEnabled;
			glm::vec3 SkyboxSunDirection;
			float SkyboxSunIntensity;
			glm::vec3 SkyboxSunColor;
			float _skyboxPadding;
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialUniformData MaterialBuffer;
		LightsUniformData LightsBuffer;
		SkyboxLightingData SkyboxLightingBuffer;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;
		Ref<UniformBuffer> SkyboxLightingUniformBuffer;

		Ref<Shader> MeshShader;
		
		// Skybox
		SkyboxRenderer SkyboxRenderer;
		glm::mat4 CurrentViewProjection;

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
		s_Data.SkyboxLightingUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::SkyboxLightingData), 5);

		// Initialize with zero lights
		s_Data.LightsBuffer.NumLights = 0;
		s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(Renderer3DData::LightsUniformData));
		
		// Initialize skybox lighting (disabled by default)
		s_Data.SkyboxLightingBuffer.SkyboxEnabled = 0.0f;
		s_Data.SkyboxLightingBuffer.SkyboxAmbient = glm::vec3(0.03f);
		s_Data.SkyboxLightingBuffer.SkyboxSunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
		s_Data.SkyboxLightingBuffer.SkyboxSunIntensity = 1.0f;
		s_Data.SkyboxLightingBuffer.SkyboxSunColor = glm::vec3(1.0f);
		s_Data.SkyboxLightingUniformBuffer->SetData(&s_Data.SkyboxLightingBuffer, sizeof(Renderer3DData::SkyboxLightingData));
		
		// Initialize skybox
		s_Data.SkyboxRenderer.Init();
	}

	void Renderer3D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		
		s_Data.SkyboxRenderer.Shutdown();
	}

	void Renderer3D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraBuffer.View = camera.GetViewMatrix();
		s_Data.CameraBuffer.Projection = camera.GetProjectionMatrix();
		s_Data.CurrentViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraPosition = glm::vec3(0.0f);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();

		glm::mat4 view = glm::inverse(transform);
		glm::mat4 projection = camera.GetProjection();
		
		s_Data.CameraBuffer.ViewProjection = projection * view;
		s_Data.CameraBuffer.View = view;
		s_Data.CameraBuffer.Projection = projection;
		s_Data.CurrentViewProjection = s_Data.CameraBuffer.ViewProjection;
		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.View = camera.GetViewMatrix();
		s_Data.CameraBuffer.Projection = camera.GetProjection();
		s_Data.CurrentViewProjection = camera.GetViewProjection();
		s_Data.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
	}

	void Renderer3D::EndScene() {
		LNX_PROFILE_FUNCTION();
	}
	
	void Renderer3D::RenderSkybox() {
		auto& skybox = s_Data.SkyboxRenderer;
		auto& settings = skybox.GetSettings();
		
		// Update skybox lighting UBO
		s_Data.SkyboxLightingBuffer.SkyboxEnabled = settings.Enabled ? 1.0f : 0.0f;
		s_Data.SkyboxLightingBuffer.SkyboxAmbient = skybox.GetAmbientColor();
		s_Data.SkyboxLightingBuffer.SkyboxSunDirection = skybox.GetSunDirection();
		s_Data.SkyboxLightingBuffer.SkyboxSunIntensity = settings.SunIntensity;
		s_Data.SkyboxLightingBuffer.SkyboxSunColor = skybox.GetSunColor();
		s_Data.SkyboxLightingUniformBuffer->SetData(&s_Data.SkyboxLightingBuffer, sizeof(Renderer3DData::SkyboxLightingData));
		
		// Render skybox
		skybox.RenderSkybox(s_Data.CurrentViewProjection, s_Data.CameraPosition);
	}
	
	SkyboxRenderer& Renderer3D::GetSkyboxRenderer() {
		return s_Data.SkyboxRenderer;
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

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID) {
		if (!meshComponent.MeshModel)
			return;

		DrawModel(transform, meshComponent.MeshModel, materialComponent, textureComponent, entityID);
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

		// No textures in this overload
		s_Data.MaterialBuffer.UseAlbedoMap = 0;
		s_Data.MaterialBuffer.UseNormalMap = 0;
		s_Data.MaterialBuffer.UseMetallicMap = 0;
		s_Data.MaterialBuffer.UseRoughnessMap = 0;
		s_Data.MaterialBuffer.UseSpecularMap = 0;
		s_Data.MaterialBuffer.UseEmissionMap = 0;
		s_Data.MaterialBuffer.UseAOMap = 0;
		s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
		s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
		s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
		s_Data.MaterialBuffer.AOMultiplier = 1.0f;

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

		// No textures in this overload
		s_Data.MaterialBuffer.UseAlbedoMap = 0;
		s_Data.MaterialBuffer.UseNormalMap = 0;
		s_Data.MaterialBuffer.UseMetallicMap = 0;
		s_Data.MaterialBuffer.UseRoughnessMap = 0;
		s_Data.MaterialBuffer.UseSpecularMap = 0;
		s_Data.MaterialBuffer.UseEmissionMap = 0;
		s_Data.MaterialBuffer.UseAOMap = 0;
		s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
		s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
		s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
		s_Data.MaterialBuffer.AOMultiplier = 1.0f;

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

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID) {
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

		// Set texture flags
		s_Data.MaterialBuffer.UseAlbedoMap = textureComponent.HasAlbedo() ? 1 : 0;
		s_Data.MaterialBuffer.UseNormalMap = textureComponent.HasNormal() ? 1 : 0;
		s_Data.MaterialBuffer.UseMetallicMap = textureComponent.HasMetallic() ? 1 : 0;
		s_Data.MaterialBuffer.UseRoughnessMap = textureComponent.HasRoughness() ? 1 : 0;
		s_Data.MaterialBuffer.UseSpecularMap = textureComponent.HasSpecular() ? 1 : 0;
		s_Data.MaterialBuffer.UseEmissionMap = textureComponent.HasEmission() ? 1 : 0;
		s_Data.MaterialBuffer.UseAOMap = textureComponent.HasAO() ? 1 : 0;

		// Set texture multipliers
		s_Data.MaterialBuffer.MetallicMultiplier = textureComponent.MetallicMultiplier;
		s_Data.MaterialBuffer.RoughnessMultiplier = textureComponent.RoughnessMultiplier;
		s_Data.MaterialBuffer.SpecularMultiplier = textureComponent.SpecularMultiplier;
		s_Data.MaterialBuffer.AOMultiplier = textureComponent.AOMultiplier;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

		// Bind textures
		s_Data.MeshShader->Bind();
		if (textureComponent.AlbedoMap) textureComponent.AlbedoMap->Bind(0);
		if (textureComponent.NormalMap) textureComponent.NormalMap->Bind(1);
		if (textureComponent.MetallicMap) textureComponent.MetallicMap->Bind(2);
		if (textureComponent.RoughnessMap) textureComponent.RoughnessMap->Bind(3);
		if (textureComponent.SpecularMap) textureComponent.SpecularMap->Bind(4);
		if (textureComponent.EmissionMap) textureComponent.EmissionMap->Bind(5);
		if (textureComponent.AOMap) textureComponent.AOMap->Bind(6);

		// Draw the model
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