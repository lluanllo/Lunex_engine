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

		// Must match the shader Material UBO layout (binding=2) exactly
		struct MaterialUniformData {
			glm::vec4 Color;                  // 16
			float Metallic;                   // 4
			float Roughness;                  // 4
			float Specular;                   // 4
			float EmissionIntensity;          // 4  = 32
			glm::vec3 EmissionColor;          // 12
			float NormalIntensity;            // 4  = 48

			int UseAlbedoMap;                 // 4
			int UseNormalMap;                 // 4
			int UseMetallicMap;               // 4
			int UseRoughnessMap;              // 4  = 64
			int UseSpecularMap;               // 4
			int UseEmissionMap;               // 4
			int UseAOMap;                     // 4
			int UseLayeredMap;                // 4  = 80

			float MetallicMultiplier;         // 4
			float RoughnessMultiplier;        // 4
			float SpecularMultiplier;         // 4
			float AOMultiplier;               // 4  = 96

			glm::vec2 UVTiling;               // 8
			glm::vec2 UVOffset;               // 8  = 112

			int LayeredChannelMetallic;       // 4
			int LayeredChannelRoughness;      // 4
			int LayeredChannelAO;             // 4
			int UseHeightMap;                 // 4  = 128

			float HeightScale;                // 4
			int UseDetailNormalMap;            // 4
			float DetailNormalScale;           // 4
			float AlphaCutoff;                // 4  = 144

			glm::vec2 DetailUVTiling;         // 8
			int AlphaMode;                    // 4
			int FlipNormalMapY;               // 4  = 160

			int AlbedoColorSpace;             // 4
			int NormalColorSpace;             // 4
			int LayeredColorSpace;            // 4
			int EmissionColorSpace;           // 4  = 176
		};

		struct ViewPosData {
			glm::vec3 ViewPos;
			float _viewPad;
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
		ViewPosData ViewPosBuffer;
		IBLUniformData IBLBuffer;

		std::vector<uint8_t> LightsBufferData;
		int CurrentLightCount = 0;
		static constexpr int MaxLights = 10000;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> ViewPosUniformBuffer;
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
		s_Data.ViewPosUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::ViewPosData), 4);
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
		s_Data.CameraPosition = glm::vec3(0.0f);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		s_Data.ViewPosBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.ViewPosBuffer._viewPad = 0.0f;
		s_Data.ViewPosUniformBuffer->SetData(&s_Data.ViewPosBuffer, sizeof(Renderer3DData::ViewPosData));

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
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		s_Data.ViewPosBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.ViewPosBuffer._viewPad = 0.0f;
		s_Data.ViewPosUniformBuffer->SetData(&s_Data.ViewPosBuffer, sizeof(Renderer3DData::ViewPosData));

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
		s_Data.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

		s_Data.ViewPosBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.ViewPosBuffer._viewPad = 0.0f;
		s_Data.ViewPosUniformBuffer->SetData(&s_Data.ViewPosBuffer, sizeof(Renderer3DData::ViewPosData));

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

		// Get material uniform data from MaterialInstance (con overrides aplicados)
		auto uniformData = materialComponent.Instance->GetUniformData();

		// Upload material data
		s_Data.MaterialBuffer.Color = uniformData.Albedo;
		s_Data.MaterialBuffer.Metallic = uniformData.Metallic;
		s_Data.MaterialBuffer.Roughness = uniformData.Roughness;
		s_Data.MaterialBuffer.Specular = uniformData.Specular;
		s_Data.MaterialBuffer.EmissionIntensity = uniformData.EmissionIntensity;
		s_Data.MaterialBuffer.EmissionColor = uniformData.EmissionColor;
		s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;

		s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
		s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
		s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
		s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
		s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
		s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
		s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;
		s_Data.MaterialBuffer.UseLayeredMap = uniformData.UseLayeredMap;

		s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
		s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
		s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
		s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

		s_Data.MaterialBuffer.UVTiling = uniformData.UVTiling;
		s_Data.MaterialBuffer.UVOffset = uniformData.UVOffset;

		s_Data.MaterialBuffer.LayeredChannelMetallic = uniformData.LayeredChannelMetallic;
		s_Data.MaterialBuffer.LayeredChannelRoughness = uniformData.LayeredChannelRoughness;
		s_Data.MaterialBuffer.LayeredChannelAO = uniformData.LayeredChannelAO;
		s_Data.MaterialBuffer.UseHeightMap = uniformData.UseHeightMap;

		s_Data.MaterialBuffer.HeightScale = uniformData.HeightScale;
		s_Data.MaterialBuffer.UseDetailNormalMap = uniformData.UseDetailNormalMap;
		s_Data.MaterialBuffer.DetailNormalScale = uniformData.DetailNormalScale;
		s_Data.MaterialBuffer.AlphaCutoff = uniformData.AlphaCutoff;

		s_Data.MaterialBuffer.DetailUVTiling = uniformData.DetailUVTiling;
		s_Data.MaterialBuffer.AlphaMode = uniformData.AlphaMode;
		s_Data.MaterialBuffer.FlipNormalMapY = uniformData.FlipNormalMapY;

		s_Data.MaterialBuffer.AlbedoColorSpace = uniformData.AlbedoColorSpace;
		s_Data.MaterialBuffer.NormalColorSpace = uniformData.NormalColorSpace;
		s_Data.MaterialBuffer.LayeredColorSpace = uniformData.LayeredColorSpace;
		s_Data.MaterialBuffer.EmissionColorSpace = uniformData.EmissionColorSpace;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(Renderer3DData::MaterialUniformData));

		// Bind shader and textures
		s_Data.MeshShader->Bind();
		materialComponent.Instance->BindTextures();

		// Draw the model
		meshComponent.MeshModel->Draw(s_Data.MeshShader);

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

	// ============================================================================
	// DrawModel overloads (internal use)
	// ============================================================================

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
		s_Data.MaterialBuffer.NormalIntensity = 1.0f;

		s_Data.MaterialBuffer.UseAlbedoMap = 0;
		s_Data.MaterialBuffer.UseNormalMap = 0;
		s_Data.MaterialBuffer.UseMetallicMap = 0;
		s_Data.MaterialBuffer.UseRoughnessMap = 0;
		s_Data.MaterialBuffer.UseSpecularMap = 0;
		s_Data.MaterialBuffer.UseEmissionMap = 0;
		s_Data.MaterialBuffer.UseAOMap = 0;
		s_Data.MaterialBuffer.UseLayeredMap = 0;
		s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
		s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
		s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
		s_Data.MaterialBuffer.AOMultiplier = 1.0f;

		s_Data.MaterialBuffer.UVTiling = glm::vec2(1.0f, 1.0f);
		s_Data.MaterialBuffer.UVOffset = glm::vec2(0.0f, 0.0f);

		s_Data.MaterialBuffer.LayeredChannelMetallic = 0;
		s_Data.MaterialBuffer.LayeredChannelRoughness = 1;
		s_Data.MaterialBuffer.LayeredChannelAO = 2;
		s_Data.MaterialBuffer.UseHeightMap = 0;

		s_Data.MaterialBuffer.HeightScale = 0.05f;
		s_Data.MaterialBuffer.UseDetailNormalMap = 0;
		s_Data.MaterialBuffer.DetailNormalScale = 1.0f;
		s_Data.MaterialBuffer.AlphaCutoff = 0.5f;

		s_Data.MaterialBuffer.DetailUVTiling = glm::vec2(4.0f, 4.0f);
		s_Data.MaterialBuffer.AlphaMode = 0;
		s_Data.MaterialBuffer.FlipNormalMapY = 0;

		s_Data.MaterialBuffer.AlbedoColorSpace = 0;  // sRGB
		s_Data.MaterialBuffer.NormalColorSpace = 1;   // Linear
		s_Data.MaterialBuffer.LayeredColorSpace = 1;  // Linear
		s_Data.MaterialBuffer.EmissionColorSpace = 0; // sRGB

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

		s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
		s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
		s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
		s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
		s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
		s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
		s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;
		s_Data.MaterialBuffer.UseLayeredMap = uniformData.UseLayeredMap;

		s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
		s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
		s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
		s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

		s_Data.MaterialBuffer.UVTiling = uniformData.UVTiling;
		s_Data.MaterialBuffer.UVOffset = uniformData.UVOffset;

		s_Data.MaterialBuffer.LayeredChannelMetallic = uniformData.LayeredChannelMetallic;
		s_Data.MaterialBuffer.LayeredChannelRoughness = uniformData.LayeredChannelRoughness;
		s_Data.MaterialBuffer.LayeredChannelAO = uniformData.LayeredChannelAO;
		s_Data.MaterialBuffer.UseHeightMap = uniformData.UseHeightMap;

		s_Data.MaterialBuffer.HeightScale = uniformData.HeightScale;
		s_Data.MaterialBuffer.UseDetailNormalMap = uniformData.UseDetailNormalMap;
		s_Data.MaterialBuffer.DetailNormalScale = uniformData.DetailNormalScale;
		s_Data.MaterialBuffer.AlphaCutoff = uniformData.AlphaCutoff;

		s_Data.MaterialBuffer.DetailUVTiling = uniformData.DetailUVTiling;
		s_Data.MaterialBuffer.AlphaMode = uniformData.AlphaMode;
		s_Data.MaterialBuffer.FlipNormalMapY = uniformData.FlipNormalMapY;

		s_Data.MaterialBuffer.AlbedoColorSpace = uniformData.AlbedoColorSpace;
		s_Data.MaterialBuffer.NormalColorSpace = uniformData.NormalColorSpace;
		s_Data.MaterialBuffer.LayeredColorSpace = uniformData.LayeredColorSpace;
		s_Data.MaterialBuffer.EmissionColorSpace = uniformData.EmissionColorSpace;

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

			s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
			s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
			s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
			s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
			s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
			s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
			s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;
			s_Data.MaterialBuffer.UseLayeredMap = uniformData.UseLayeredMap;

			s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
			s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
			s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
			s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

			s_Data.MaterialBuffer.UVTiling = uniformData.UVTiling;
			s_Data.MaterialBuffer.UVOffset = uniformData.UVOffset;

			s_Data.MaterialBuffer.LayeredChannelMetallic = uniformData.LayeredChannelMetallic;
			s_Data.MaterialBuffer.LayeredChannelRoughness = uniformData.LayeredChannelRoughness;
			s_Data.MaterialBuffer.LayeredChannelAO = uniformData.LayeredChannelAO;
			s_Data.MaterialBuffer.UseHeightMap = uniformData.UseHeightMap;

			s_Data.MaterialBuffer.HeightScale = uniformData.HeightScale;
			s_Data.MaterialBuffer.UseDetailNormalMap = uniformData.UseDetailNormalMap;
			s_Data.MaterialBuffer.DetailNormalScale = uniformData.DetailNormalScale;
			s_Data.MaterialBuffer.AlphaCutoff = uniformData.AlphaCutoff;

			s_Data.MaterialBuffer.DetailUVTiling = uniformData.DetailUVTiling;
			s_Data.MaterialBuffer.AlphaMode = uniformData.AlphaMode;

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