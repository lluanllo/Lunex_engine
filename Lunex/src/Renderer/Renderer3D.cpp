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

		// Ray Tracing Settings (matches shader layout binding = 6)
		struct RayTracingSettingsData {
			int EnableRayTracedShadows;
			float ShadowRayBias;
			float ShadowSoftness;
			int ShadowSamplesPerLight;
			
			float MaxShadowDistance;
			float ShadowFadeDistance;
			int UseContactHardening;
			float ContactHardeningScale;
			
			int EnableGroundPlane;      // NEW
			float GroundPlaneY;         // NEW
			int VisualizeShadowRays;    // NEW
			float _rtPadding;
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialUniformData MaterialBuffer;
		LightsUniformData LightsBuffer;
		RayTracingSettingsData RayTracingSettingsBuffer;
		
		RayTracingSettings RayTracingSettingsPublic; // Public-facing settings

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;
		Ref<UniformBuffer> RayTracingUniformBuffer;

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
		s_Data.RayTracingUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::RayTracingSettingsData), 6);

		// Initialize with zero lights
		s_Data.LightsBuffer.NumLights = 0;
		s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(Renderer3DData::LightsUniformData));

		// Initialize ray tracing settings with default values
		s_Data.RayTracingSettingsBuffer.EnableRayTracedShadows = 1;
		s_Data.RayTracingSettingsBuffer.ShadowRayBias = 0.001f;
		s_Data.RayTracingSettingsBuffer.ShadowSoftness = 0.5f;
		s_Data.RayTracingSettingsBuffer.ShadowSamplesPerLight = 4;
		
		s_Data.RayTracingSettingsBuffer.MaxShadowDistance = 100.0f;
		s_Data.RayTracingSettingsBuffer.ShadowFadeDistance = 10.0f;
		s_Data.RayTracingSettingsBuffer.UseContactHardening = 1;
		s_Data.RayTracingSettingsBuffer.ContactHardeningScale = 1.0f;
		
		s_Data.RayTracingSettingsBuffer.EnableGroundPlane = 1;
		s_Data.RayTracingSettingsBuffer.GroundPlaneY = 0.0f;
		s_Data.RayTracingSettingsBuffer.VisualizeShadowRays = 0;
		s_Data.RayTracingSettingsBuffer._rtPadding = 0.0f;

		s_Data.RayTracingUniformBuffer->SetData(&s_Data.RayTracingSettingsBuffer, sizeof(Renderer3DData::RayTracingSettingsData));
		
		// Initialize public settings
		s_Data.RayTracingSettingsPublic.EnableRayTracedShadows = true;
		s_Data.RayTracingSettingsPublic.ShadowRayBias = 0.001f;
		s_Data.RayTracingSettingsPublic.ShadowSoftness = 0.5f;
		s_Data.RayTracingSettingsPublic.ShadowSamplesPerLight = 4;
		s_Data.RayTracingSettingsPublic.MaxShadowDistance = 100.0f;
		s_Data.RayTracingSettingsPublic.ShadowFadeDistance = 10.0f;
		s_Data.RayTracingSettingsPublic.UseContactHardening = true;
		s_Data.RayTracingSettingsPublic.ContactHardeningScale = 1.0f;
		s_Data.RayTracingSettingsPublic.EnableGroundPlane = true;
		s_Data.RayTracingSettingsPublic.GroundPlaneY = 0.0f;
		s_Data.RayTracingSettingsPublic.VisualizeShadowRays = false;
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

	RayTracingSettings& Renderer3D::GetRayTracingSettings() {
		return s_Data.RayTracingSettingsPublic;
	}

	void Renderer3D::SetRayTracingSettings(const RayTracingSettings& settings) {
		s_Data.RayTracingSettingsPublic = settings;
		UpdateRayTracingBuffer();
	}

	void Renderer3D::UpdateRayTracingBuffer() {
		// Copy from public settings to GPU buffer
		s_Data.RayTracingSettingsBuffer.EnableRayTracedShadows = s_Data.RayTracingSettingsPublic.EnableRayTracedShadows ? 1 : 0;
		s_Data.RayTracingSettingsBuffer.ShadowRayBias = s_Data.RayTracingSettingsPublic.ShadowRayBias;
		s_Data.RayTracingSettingsBuffer.ShadowSoftness = s_Data.RayTracingSettingsPublic.ShadowSoftness;
		s_Data.RayTracingSettingsBuffer.ShadowSamplesPerLight = s_Data.RayTracingSettingsPublic.ShadowSamplesPerLight;
		
		s_Data.RayTracingSettingsBuffer.MaxShadowDistance = s_Data.RayTracingSettingsPublic.MaxShadowDistance;
		s_Data.RayTracingSettingsBuffer.ShadowFadeDistance = s_Data.RayTracingSettingsPublic.ShadowFadeDistance;
		s_Data.RayTracingSettingsBuffer.UseContactHardening = s_Data.RayTracingSettingsPublic.UseContactHardening ? 1 : 0;
		s_Data.RayTracingSettingsBuffer.ContactHardeningScale = s_Data.RayTracingSettingsPublic.ContactHardeningScale;
		
		s_Data.RayTracingSettingsBuffer.EnableGroundPlane = s_Data.RayTracingSettingsPublic.EnableGroundPlane ? 1 : 0;
		s_Data.RayTracingSettingsBuffer.GroundPlaneY = s_Data.RayTracingSettingsPublic.GroundPlaneY;
		s_Data.RayTracingSettingsBuffer.VisualizeShadowRays = s_Data.RayTracingSettingsPublic.VisualizeShadowRays ? 1 : 0;
		
		// Upload to GPU
		s_Data.RayTracingUniformBuffer->SetData(&s_Data.RayTracingSettingsBuffer, sizeof(Renderer3DData::RayTracingSettingsData));
	}
}