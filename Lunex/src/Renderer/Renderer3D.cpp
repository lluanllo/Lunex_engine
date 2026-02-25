#include "stpch.h"
#include "Renderer3D.h"

#include "RHI/RHI.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/StorageBuffer.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Resources/Mesh/Model.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Lighting/LightTypes.h"
#include "Scene/Lighting/LightSystem.h"
#include "Log/Log.h"

#include <glad/glad.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lunex {
	struct Renderer3DData {
		struct CameraData {
			glm::mat4 ViewProjection;
			glm::mat4 View;
			glm::mat4 Projection;
			glm::vec3 ViewPos;
			float _padding = 0.0f;
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
			float NormalIntensity;
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

			// Detail normals & layered
			int DetailNormalCount;
			int UseLayeredTexture;
			int LayeredMetallicChannel;
			int LayeredRoughnessChannel;

			int LayeredAOChannel;
			int LayeredUseMetallic;
			int LayeredUseRoughness;
			int LayeredUseAO;

			// std140 requires vec4 to be 16-byte aligned; the 8 ints above
			// end at a non-16-byte-aligned offset, so we need explicit padding
			float _detailPad0;

			glm::vec4 DetailNormalIntensities;
			glm::vec4 DetailNormalTilingX;
			glm::vec4 DetailNormalTilingY;
		};

		struct IBLUniformData {
			float Intensity;
			float Rotation;
			int UseIBL;
			float _padding;
		};

		struct LightsStorageData {
			int NumLights;
			float _padding1, _padding2, _padding3;
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialUniformData MaterialBuffer;
		IBLUniformData IBLBuffer;

		std::vector<uint8_t> LightsBufferData;
		int CurrentLightCount = 0;
		static constexpr int MaxLights = 10000;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> IBLUniformBuffer;
		Ref<StorageBuffer> LightsStorageBuffer;

		Ref<Shader> MeshShader;

		glm::vec3 CameraPosition = { 0.0f, 0.0f, 0.0f };

		Ref<EnvironmentMap> CurrentEnvironment;

		Renderer3D::Statistics Stats;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init() {
		LNX_PROFILE_FUNCTION();

		s_Data.MeshShader = Shader::Create("assets/shaders/Mesh3D.glsl");

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);
		s_Data.TransformUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::TransformData), 1);
		s_Data.MaterialUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::MaterialUniformData), 2);
		s_Data.IBLUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::IBLUniformData), 5);

		// Create storage buffer for lights (header + MaxLights * LightData)
		uint32_t lightsBufferSize = sizeof(Renderer3DData::LightsStorageData) +
			(s_Data.MaxLights * sizeof(LightData));
		s_Data.LightsStorageBuffer = StorageBuffer::Create(lightsBufferSize, 3);

		s_Data.LightsBufferData.resize(lightsBufferSize);

		s_Data.CurrentLightCount = 0;
		Renderer3DData::LightsStorageData* header =
			reinterpret_cast<Renderer3DData::LightsStorageData*>(s_Data.LightsBufferData.data());
		header->NumLights = 0;
		s_Data.LightsStorageBuffer->SetData(s_Data.LightsBufferData.data(), lightsBufferSize);

		s_Data.IBLBuffer.UseIBL = 0;
		s_Data.IBLBuffer.Intensity = 1.0f;
		s_Data.IBLBuffer.Rotation = 0.0f;
		s_Data.IBLUniformBuffer->SetData(&s_Data.IBLBuffer, sizeof(Renderer3DData::IBLUniformData));

		GridRenderer::Init();

		// Initialize Shadow System
		ShadowSystem::Get().Initialize();
	}

	void Renderer3D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		ShadowSystem::Get().Shutdown();
		GridRenderer::Shutdown();
	}

	void Renderer3D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraBuffer.View = glm::mat4(1.0f);
		s_Data.CameraBuffer.Projection = glm::mat4(1.0f);
		s_Data.CameraBuffer.ViewPos = glm::vec3(0.0f);
		s_Data.CameraPosition = glm::vec3(0.0f);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		auto globalEnv = SkyboxRenderer::GetGlobalEnvironment();
		if (globalEnv && globalEnv->IsLoaded()) {
			BindEnvironment(globalEnv);
		}
		else {
			UnbindEnvironment();
		}
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();
		glm::mat4 viewMatrix = glm::inverse(transform);
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * viewMatrix;
		s_Data.CameraBuffer.View = viewMatrix;
		s_Data.CameraBuffer.Projection = camera.GetProjection();
		s_Data.CameraBuffer.ViewPos = glm::vec3(transform[3]);
		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		auto globalEnv = SkyboxRenderer::GetGlobalEnvironment();
		if (globalEnv && globalEnv->IsLoaded()) {
			BindEnvironment(globalEnv);
		}
		else {
			UnbindEnvironment();
		}

		// Bind shadow atlas for reading during scene rendering
		ShadowSystem::Get().BindForSceneRendering();
	}

	void Renderer3D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.View = camera.GetViewMatrix();
		s_Data.CameraBuffer.Projection = camera.GetProjection();
		s_Data.CameraBuffer.ViewPos = camera.GetPosition();
		s_Data.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		auto globalEnv = SkyboxRenderer::GetGlobalEnvironment();
		if (globalEnv && globalEnv->IsLoaded()) {
			BindEnvironment(globalEnv);
		}
		else {
			UnbindEnvironment();
		}

		// Bind shadow atlas for reading during scene rendering
		ShadowSystem::Get().BindForSceneRendering();
	}

	void Renderer3D::EndScene() {
		LNX_PROFILE_FUNCTION();
	}

	void Renderer3D::BindEnvironment(const Ref<EnvironmentMap>& environment) {
		if (!environment || !environment->IsLoaded()) {
			UnbindEnvironment();
			return;
		}

		s_Data.CurrentEnvironment = environment;

		if (environment->GetIrradianceMap()) {
			environment->GetIrradianceMap()->Bind(8);
		}
		if (environment->GetPrefilteredMap()) {
			environment->GetPrefilteredMap()->Bind(9);
		}
		if (environment->GetBRDFLUT()) {
			environment->GetBRDFLUT()->Bind(10);
		}

		s_Data.IBLBuffer.UseIBL = 1;
		s_Data.IBLBuffer.Intensity = environment->GetIntensity();
		s_Data.IBLBuffer.Rotation = environment->GetRotation();
		s_Data.IBLUniformBuffer->SetData(&s_Data.IBLBuffer, sizeof(Renderer3DData::IBLUniformData));
	}

	void Renderer3D::UnbindEnvironment() {
		s_Data.CurrentEnvironment = nullptr;
		s_Data.IBLBuffer.UseIBL = 0;
		s_Data.IBLBuffer.Intensity = 1.0f;
		s_Data.IBLBuffer.Rotation = 0.0f;
		s_Data.IBLUniformBuffer->SetData(&s_Data.IBLBuffer, sizeof(Renderer3DData::IBLUniformData));
	}

	void Renderer3D::UpdateLights(Scene* scene) {
		LNX_PROFILE_FUNCTION();

		// ? SYNC WITH LIGHT SYSTEM for sun light detection and skybox sync
		LightSystem::Get().SyncFromScene(scene);

		auto view = scene->GetAllEntitiesWith<TransformComponent, LightComponent>();

		int lightCount = 0;
		view.each([&](auto entity, auto& transform, auto& light) {
			if (lightCount < s_Data.MaxLights) {
				lightCount++;
			}
			});

		if (lightCount > s_Data.MaxLights) {
			LNX_LOG_WARN("Light count exceeded maximum of {0}. Some lights will not be rendered.", s_Data.MaxLights);
			lightCount = s_Data.MaxLights;
		}

		Renderer3DData::LightsStorageData* header =
			reinterpret_cast<Renderer3DData::LightsStorageData*>(s_Data.LightsBufferData.data());
		header->NumLights = lightCount;

		// FIXED: Use LightData directly instead of Light::LightData
		LightData* lightsArray = reinterpret_cast<LightData*>(
			s_Data.LightsBufferData.data() + sizeof(Renderer3DData::LightsStorageData)
			);

		int lightIndex = 0;
		view.each([&](auto entity, auto& transform, auto& light) {
			if (lightIndex >= s_Data.MaxLights) return;

			glm::vec3 position = transform.Translation;
			glm::vec3 direction = glm::normalize(
				glm::rotate(glm::quat(transform.Rotation), glm::vec3(0.0f, 0.0f, -1.0f))
			);

			lightsArray[lightIndex] = light.LightInstance->GetLightData(position, direction);
			lightIndex++;
			});

		s_Data.CurrentLightCount = lightCount;

		// FIXED: Use LightData directly
		uint32_t dataSize = sizeof(Renderer3DData::LightsStorageData) +
			(lightCount * sizeof(LightData));
		s_Data.LightsStorageBuffer->SetData(s_Data.LightsBufferData.data(), dataSize);
	}

	void Renderer3D::UpdateShadows(Scene* scene, const EditorCamera& camera) {
		if (!scene) return;
		ShadowSystem::Get().Update(scene, camera);
	}

	void Renderer3D::UpdateShadows(Scene* scene, const Camera& camera, const glm::mat4& cameraTransform) {
		if (!scene) return;
		ShadowSystem::Get().Update(scene, camera, cameraTransform);
	}

	// ============================================================================
	// NEW MATERIAL SYSTEM - DrawMesh overloads
	// ============================================================================

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID) {
		if (!meshComponent.MeshModel)
			return;

		// Usar material por defecto si no hay MaterialComponent
		auto defaultMaterial = MaterialRegistry::Get().GetDefaultMaterial();
		DrawModel(transform, meshComponent.MeshModel, defaultMaterial->GetAlbedo(), entityID);
	}

	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, int entityID) {
		if (!meshComponent.MeshModel || !materialComponent.Instance)
			return;

		const_cast<Model*>(meshComponent.MeshModel.get())->SetEntityID(entityID);

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Get base material uniform data from MaterialInstance
		auto uniformData = materialComponent.Instance->GetUniformData();

		s_Data.MeshShader->Bind();

		// Check if any sub-mesh has its own textures (from GLTF/Assimp import)
		bool hasPerMeshTextures = false;
		for (const auto& mesh : meshComponent.MeshModel->GetMeshes()) {
			if (mesh->HasAnyMeshTextures()) {
				hasPerMeshTextures = true;
				break;
			}
		}

		if (!hasPerMeshTextures) {
			// Simple path: single material for whole model
			s_Data.MaterialBuffer.Color = uniformData.Albedo;
			s_Data.MaterialBuffer.Metallic = uniformData.Metallic;
			s_Data.MaterialBuffer.Roughness = uniformData.Roughness;
			s_Data.MaterialBuffer.Specular = uniformData.Specular;
			s_Data.MaterialBuffer.EmissionIntensity = uniformData.EmissionIntensity;
			s_Data.MaterialBuffer.EmissionColor = uniformData.EmissionColor;
			s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;
			s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

			s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
			s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
			s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
			s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
			s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
			s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
			s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;

			s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
			s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
			s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
			s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

			s_Data.MaterialBuffer.DetailNormalCount = uniformData.DetailNormalCount;
			s_Data.MaterialBuffer.UseLayeredTexture = uniformData.UseLayeredTexture;
			s_Data.MaterialBuffer.LayeredMetallicChannel = uniformData.LayeredMetallicChannel;
			s_Data.MaterialBuffer.LayeredRoughnessChannel = uniformData.LayeredRoughnessChannel;
			s_Data.MaterialBuffer.LayeredAOChannel = uniformData.LayeredAOChannel;
			s_Data.MaterialBuffer.LayeredUseMetallic = uniformData.LayeredUseMetallic;
			s_Data.MaterialBuffer.LayeredUseRoughness = uniformData.LayeredUseRoughness;
			s_Data.MaterialBuffer.LayeredUseAO = uniformData.LayeredUseAO;
			s_Data.MaterialBuffer.DetailNormalIntensities = uniformData.DetailNormalIntensities;
			s_Data.MaterialBuffer.DetailNormalTilingX = uniformData.DetailNormalTilingX;
			s_Data.MaterialBuffer.DetailNormalTilingY = uniformData.DetailNormalTilingY;

			s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

			materialComponent.Instance->BindTextures();
			meshComponent.MeshModel->Draw(s_Data.MeshShader);
		}
		else {
			// Per-mesh texture path: each sub-mesh may have different textures and material properties
			for (const auto& mesh : meshComponent.MeshModel->GetMeshes()) {
				const auto& meshMatData = mesh->GetMaterialData();
				uint32_t texFlags = mesh->GetTextureFlags();

				// Use per-mesh material data from GLTF as base
				s_Data.MaterialBuffer.Color = meshMatData.BaseColor;
				s_Data.MaterialBuffer.Metallic = meshMatData.Metallic;
				s_Data.MaterialBuffer.Roughness = meshMatData.Roughness;
				s_Data.MaterialBuffer.Specular = uniformData.Specular;
				s_Data.MaterialBuffer.EmissionIntensity = meshMatData.EmissionIntensity;
				s_Data.MaterialBuffer.EmissionColor = meshMatData.EmissionColor;
				s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;
				s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;
				s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
				s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
				s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
				s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;
				s_Data.MaterialBuffer.DetailNormalCount = 0;
				s_Data.MaterialBuffer.UseLayeredTexture = 0;
				s_Data.MaterialBuffer.LayeredMetallicChannel = 0;
				s_Data.MaterialBuffer.LayeredRoughnessChannel = 0;
				s_Data.MaterialBuffer.LayeredAOChannel = 0;
				s_Data.MaterialBuffer.LayeredUseMetallic = 0;
				s_Data.MaterialBuffer.LayeredUseRoughness = 0;
				s_Data.MaterialBuffer.LayeredUseAO = 0;

				// Override texture flags from per-mesh textures
				s_Data.MaterialBuffer.UseAlbedoMap = ((texFlags & MeshTexFlag_Diffuse) || uniformData.UseAlbedoMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseNormalMap = ((texFlags & MeshTexFlag_Normal) || uniformData.UseNormalMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseMetallicMap = ((texFlags & MeshTexFlag_Metallic) || uniformData.UseMetallicMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseRoughnessMap = ((texFlags & MeshTexFlag_Roughness) || uniformData.UseRoughnessMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseSpecularMap = ((texFlags & MeshTexFlag_Specular) || uniformData.UseSpecularMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseEmissionMap = ((texFlags & MeshTexFlag_Emissive) || uniformData.UseEmissionMap) ? 1 : 0;
				s_Data.MaterialBuffer.UseAOMap = ((texFlags & MeshTexFlag_AO) || uniformData.UseAOMap) ? 1 : 0;

				s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

				// Only unbind slots not provided by this mesh
				if (!(texFlags & MeshTexFlag_Diffuse) && !uniformData.UseAlbedoMap) glBindTextureUnit(0, 0);
				if (!(texFlags & MeshTexFlag_Normal) && !uniformData.UseNormalMap) glBindTextureUnit(1, 0);
				if (!(texFlags & MeshTexFlag_Metallic) && !uniformData.UseMetallicMap) glBindTextureUnit(2, 0);
				if (!(texFlags & MeshTexFlag_Roughness) && !uniformData.UseRoughnessMap) glBindTextureUnit(3, 0);
				if (!(texFlags & MeshTexFlag_Specular) && !uniformData.UseSpecularMap) glBindTextureUnit(4, 0);
				if (!(texFlags & MeshTexFlag_Emissive) && !uniformData.UseEmissionMap) glBindTextureUnit(5, 0);
				if (!(texFlags & MeshTexFlag_AO) && !uniformData.UseAOMap) glBindTextureUnit(6, 0);

				materialComponent.Instance->BindTextures();

				// Bind per-mesh textures (overrides MaterialComponent textures)
				mesh->Draw(s_Data.MeshShader);
			}
		}

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)meshComponent.MeshModel->GetMeshes().size();

		for (const auto& mesh : meshComponent.MeshModel->GetMeshes()) {
		 s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	// DEPRECATED: Este método mantiene compatibilidad con TextureComponent legacy
	void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID) {
		// Por compatibilidad temporal, renderizar con MaterialComponent ignorando TextureComponent
		LNX_LOG_WARN("DrawMesh with TextureComponent is deprecated. Migrate to MaterialAsset.");
		DrawMesh(transform, meshComponent, materialComponent, entityID);
	}

	// ================================================================================
	// DrawModel overloads (internal use)
	// ================================================================================

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();

		if (!model)
			return;

		const_cast<Model*>(model.get())->SetEntityID(entityID);

		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		s_Data.MeshShader->Bind();

		// Check if any sub-mesh has its own textures
		bool hasPerMeshTextures = false;
		for (const auto& mesh : model->GetMeshes()) {
			if (mesh->HasAnyMeshTextures()) {
				hasPerMeshTextures = true;
				break;
			}
		}

		if (!hasPerMeshTextures) {
			// Single mesh or primitives: use first mesh's material data if available
			const auto& meshes = model->GetMeshes();
			if (!meshes.empty()) {
				const auto& meshMatData = meshes[0]->GetMaterialData();
				memset(&s_Data.MaterialBuffer, 0, sizeof(Renderer3DData::MaterialUniformData));
				s_Data.MaterialBuffer.Color = meshMatData.BaseColor * color;
				s_Data.MaterialBuffer.Metallic = meshMatData.Metallic;
				s_Data.MaterialBuffer.Roughness = meshMatData.Roughness;
				s_Data.MaterialBuffer.Specular = 0.5f;
				s_Data.MaterialBuffer.EmissionIntensity = meshMatData.EmissionIntensity;
				s_Data.MaterialBuffer.EmissionColor = meshMatData.EmissionColor;
				s_Data.MaterialBuffer.NormalIntensity = 1.0f;
				s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
				s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
				s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
				s_Data.MaterialBuffer.AOMultiplier = 1.0f;
				s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;
			}
			else {
				memset(&s_Data.MaterialBuffer, 0, sizeof(Renderer3DData::MaterialUniformData));
				s_Data.MaterialBuffer.Color = color;
				s_Data.MaterialBuffer.Roughness = 0.5f;
				s_Data.MaterialBuffer.Specular = 0.5f;
				s_Data.MaterialBuffer.NormalIntensity = 1.0f;
				s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
				s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
				s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
				s_Data.MaterialBuffer.AOMultiplier = 1.0f;
				s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;
			}

			s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

			model->Draw(s_Data.MeshShader);
		}
		else {
			for (const auto& mesh : model->GetMeshes()) {
				const auto& meshMatData = mesh->GetMaterialData();
				uint32_t texFlags = mesh->GetTextureFlags();

				memset(&s_Data.MaterialBuffer, 0, sizeof(Renderer3DData::MaterialUniformData));
				s_Data.MaterialBuffer.Color = meshMatData.BaseColor * color;
				s_Data.MaterialBuffer.Metallic = meshMatData.Metallic;
				s_Data.MaterialBuffer.Roughness = meshMatData.Roughness;
				s_Data.MaterialBuffer.Specular = 0.5f;
				s_Data.MaterialBuffer.EmissionIntensity = meshMatData.EmissionIntensity;
				s_Data.MaterialBuffer.EmissionColor = meshMatData.EmissionColor;
				s_Data.MaterialBuffer.NormalIntensity = 1.0f;
				s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
			 s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
				s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
			 s_Data.MaterialBuffer.AOMultiplier = 1.0f;
				s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

				s_Data.MaterialBuffer.UseAlbedoMap = (texFlags & MeshTexFlag_Diffuse) ? 1 : 0;
				s_Data.MaterialBuffer.UseNormalMap = (texFlags & MeshTexFlag_Normal) ? 1 : 0;
				s_Data.MaterialBuffer.UseMetallicMap = (texFlags & MeshTexFlag_Metallic) ? 1 : 0;
				s_Data.MaterialBuffer.UseRoughnessMap = (texFlags & MeshTexFlag_Roughness) ? 1 : 0;
				s_Data.MaterialBuffer.UseSpecularMap = (texFlags & MeshTexFlag_Specular) ? 1 : 0;
				s_Data.MaterialBuffer.UseEmissionMap = (texFlags & MeshTexFlag_Emissive) ? 1 : 0;
				s_Data.MaterialBuffer.UseAOMap = (texFlags & MeshTexFlag_AO) ? 1 : 0;

				s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

				// Only unbind slots not provided by this mesh
				if (!(texFlags & MeshTexFlag_Diffuse)) glBindTextureUnit(0, 0);
				if (!(texFlags & MeshTexFlag_Normal)) glBindTextureUnit(1, 0);
				if (!(texFlags & MeshTexFlag_Metallic)) glBindTextureUnit(2, 0);
				if (!(texFlags & MeshTexFlag_Roughness)) glBindTextureUnit(3, 0);
				if (!(texFlags & MeshTexFlag_Specular)) glBindTextureUnit(4, 0);
				if (!(texFlags & MeshTexFlag_Emissive)) glBindTextureUnit(5, 0);
				if (!(texFlags & MeshTexFlag_AO)) glBindTextureUnit(6, 0);

				mesh->Draw(s_Data.MeshShader);
			}
		}

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)model->GetMeshes().size();
		for (const auto& mesh : model->GetMeshes()) {
			s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, int entityID) {
		LNX_PROFILE_FUNCTION();

		if (!model || model->GetMeshes().empty() || !materialComponent.Instance)
			return;

		const_cast<Model*>(model.get())->SetEntityID(entityID);

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Get material uniform data from MaterialInstance
		auto uniformData = materialComponent.Instance->GetUniformData();

		// Copy to s_Data.MaterialBuffer
		s_Data.MaterialBuffer.Color = uniformData.Albedo;
		s_Data.MaterialBuffer.Metallic = uniformData.Metallic;
		s_Data.MaterialBuffer.Roughness = uniformData.Roughness;
		s_Data.MaterialBuffer.Specular = uniformData.Specular;
		s_Data.MaterialBuffer.EmissionIntensity = uniformData.EmissionIntensity;
		s_Data.MaterialBuffer.EmissionColor = uniformData.EmissionColor;
		s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

		s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
		s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
		s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
		s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
		s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
		s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
		s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;

		s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
		s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
		s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
		s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

		s_Data.MaterialBuffer.DetailNormalCount = uniformData.DetailNormalCount;
		s_Data.MaterialBuffer.UseLayeredTexture = uniformData.UseLayeredTexture;
		s_Data.MaterialBuffer.LayeredMetallicChannel = uniformData.LayeredMetallicChannel;
		s_Data.MaterialBuffer.LayeredRoughnessChannel = uniformData.LayeredRoughnessChannel;
		s_Data.MaterialBuffer.LayeredAOChannel = uniformData.LayeredAOChannel;
		s_Data.MaterialBuffer.LayeredUseMetallic = uniformData.LayeredUseMetallic;
		s_Data.MaterialBuffer.LayeredUseRoughness = uniformData.LayeredUseRoughness;
		s_Data.MaterialBuffer.LayeredUseAO = uniformData.LayeredUseAO;
		s_Data.MaterialBuffer.DetailNormalIntensities = uniformData.DetailNormalIntensities;
		s_Data.MaterialBuffer.DetailNormalTilingX = uniformData.DetailNormalTilingX;
		s_Data.MaterialBuffer.DetailNormalTilingY = uniformData.DetailNormalTilingY;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

		// Bind shader and textures
		s_Data.MeshShader->Bind();
		materialComponent.Instance->BindTextures();

		// Draw the model
		model->Draw(s_Data.MeshShader);

		s_Data.Stats.DrawCalls++;
		s_Data.Stats.MeshCount += (uint32_t)model->GetMeshes().size();

		for (const auto& mesh : model->GetMeshes()) {
		    s_Data.Stats.TriangleCount += (uint32_t)mesh->GetIndices().size() / 3;
		}
	}

	// DEPRECATED: Para compatibilidad con TextureComponent legacy
	void Renderer3D::DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID) {
		LNX_PROFILE_FUNCTION();

		if (!model || model->GetMeshes().empty())
			return;

		const_cast<Model*>(model.get())->SetEntityID(entityID);

		// Update Transform uniform buffer
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(Renderer3DData::TransformData));

		// Get material from MaterialComponent (ignore TextureComponent - deprecated)
		if (materialComponent.Instance) {
			auto uniformData = materialComponent.Instance->GetUniformData();

			s_Data.MaterialBuffer.Color = uniformData.Albedo;
			s_Data.MaterialBuffer.Metallic = uniformData.Metallic;
		 s_Data.MaterialBuffer.Roughness = uniformData.Roughness;
			s_Data.MaterialBuffer.Specular = uniformData.Specular;
			s_Data.MaterialBuffer.EmissionIntensity = uniformData.EmissionIntensity;
			s_Data.MaterialBuffer.EmissionColor = uniformData.EmissionColor;
			s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;
			s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

			s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
			s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
			s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
			s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
			s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
			s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
			s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;

			s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
			s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
			s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
		    s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

			s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

			// Bind shader and textures from MaterialInstance
			s_Data.MeshShader->Bind();
			materialComponent.Instance->BindTextures();
		}

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