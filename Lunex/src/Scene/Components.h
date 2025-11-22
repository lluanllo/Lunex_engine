#pragma once

#include "SceneCamera.h"
#include "Core/UUID.h"
#include "Renderer/Texture.h"
#include "Renderer/Model.h"
#include "Renderer/Material.h"
#include "Renderer/Light.h"
#include "Log/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lunex {
	struct IDComponent {
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		// Permite emplace con UUID (evita el static_assert/invoke)
		IDComponent(UUID uuid)
			: ID(uuid) {
		}
	};

	struct TagComponent {
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {
		}
	};

	struct TransformComponent {
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {
		}

		glm::mat4 GetTransform() const {
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct SpriteRendererComponent {
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {
		}
	};

	struct CircleRendererComponent {
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct MeshComponent {
		Ref<Model> MeshModel;
		ModelType Type = ModelType::Cube;
		std::string FilePath;
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		MeshComponent() {
			CreatePrimitive(ModelType::Cube);
		}
		
		MeshComponent(const MeshComponent&) = default;
		
		MeshComponent(ModelType type)
			: Type(type) {
			CreatePrimitive(type);
		}

		void CreatePrimitive(ModelType type) {
			Type = type;
			switch (type) {
			case ModelType::Cube:
				MeshModel = Model::CreateCube();
				break;
			case ModelType::Sphere:
				MeshModel = Model::CreateSphere();
				break;
			case ModelType::Plane:
				MeshModel = Model::CreatePlane();
				break;
			case ModelType::Cylinder:
				MeshModel = Model::CreateCylinder();
				break;
			case ModelType::FromFile:
				// Will be loaded from FilePath
				break;
			}
		}

		void LoadFromFile(const std::string& path) {
			FilePath = path;
			Type = ModelType::FromFile;
			MeshModel = CreateRef<Model>(path);
		}
	};

	struct MaterialComponent {
		Ref<Material> MaterialInstance;

		MaterialComponent()
			: MaterialInstance(CreateRef<Material>()) {
		}

		MaterialComponent(const MaterialComponent& other)
			: MaterialInstance(CreateRef<Material>(*other.MaterialInstance)) {
		}

		MaterialComponent(const glm::vec4& color)
			: MaterialInstance(CreateRef<Material>(color)) {
		}

		// Material properties accessors
		void SetColor(const glm::vec4& color) { MaterialInstance->SetColor(color); }
		void SetMetallic(float metallic) { MaterialInstance->SetMetallic(metallic); }
		void SetRoughness(float roughness) { MaterialInstance->SetRoughness(roughness); }
		void SetSpecular(float specular) { MaterialInstance->SetSpecular(specular); }
		void SetEmissionColor(const glm::vec3& color) { MaterialInstance->SetEmissionColor(color); }
		void SetEmissionIntensity(float intensity) { MaterialInstance->SetEmissionIntensity(intensity); }

		const glm::vec4& GetColor() const { return MaterialInstance->GetColor(); }
		float GetMetallic() const { return MaterialInstance->GetMetallic(); }
		float GetRoughness() const { return MaterialInstance->GetRoughness(); }
		float GetSpecular() const { return MaterialInstance->GetSpecular(); }
		const glm::vec3& GetEmissionColor() const { return MaterialInstance->GetEmissionColor(); }
		float GetEmissionIntensity() const { return MaterialInstance->GetEmissionIntensity(); }
	};

	struct TextureComponent {
		// PBR Texture Maps
		Ref<Texture2D> AlbedoMap;
		Ref<Texture2D> NormalMap;
		Ref<Texture2D> MetallicMap;
		Ref<Texture2D> RoughnessMap;
		Ref<Texture2D> SpecularMap;
		Ref<Texture2D> EmissionMap;
		Ref<Texture2D> AOMap;

		// Texture paths for serialization
		std::string AlbedoPath;
		std::string NormalPath;
		std::string MetallicPath;
		std::string RoughnessPath;
		std::string SpecularPath;
		std::string EmissionPath;
		std::string AOPath;

		// Texture multipliers/exposure
		float MetallicMultiplier = 1.0f;
		float RoughnessMultiplier = 1.0f;
		float SpecularMultiplier = 1.0f;
		float AOMultiplier = 1.0f;

		TextureComponent() = default;
		TextureComponent(const TextureComponent&) = default;

		// Load texture helpers
		void LoadAlbedo(const std::string& path) {
			AlbedoPath = path;
			AlbedoMap = Texture2D::Create(path);
		}

		void LoadNormal(const std::string& path) {
			NormalPath = path;
			NormalMap = Texture2D::Create(path);
		}

		void LoadMetallic(const std::string& path) {
			MetallicPath = path;
			MetallicMap = Texture2D::Create(path);
		}

		void LoadRoughness(const std::string& path) {
			RoughnessPath = path;
			RoughnessMap = Texture2D::Create(path);
		}

		void LoadSpecular(const std::string& path) {
			SpecularPath = path;
			SpecularMap = Texture2D::Create(path);
		}

		void LoadEmission(const std::string& path) {
			EmissionPath = path;
			EmissionMap = Texture2D::Create(path);
		}

		void LoadAO(const std::string& path) {
			AOPath = path;
			AOMap = Texture2D::Create(path);
		}

		// Check if any texture is loaded
		bool HasAnyTexture() const {
			return AlbedoMap || NormalMap || MetallicMap || 
				   RoughnessMap || SpecularMap || EmissionMap || AOMap;
		}

		bool HasAlbedo() const { return AlbedoMap != nullptr; }
		bool HasNormal() const { return NormalMap != nullptr; }
		bool HasMetallic() const { return MetallicMap != nullptr; }
		bool HasRoughness() const { return RoughnessMap != nullptr; }
		bool HasSpecular() const { return SpecularMap != nullptr; }
		bool HasEmission() const { return EmissionMap != nullptr; }
		bool HasAO() const { return AOMap != nullptr; }
	};

	struct LightComponent {
		Ref<Light> LightInstance;
		Ref<Texture2D> IconTexture;

		LightComponent()
			: LightInstance(CreateRef<Light>()) {
			LoadIcon();
		}

		LightComponent(const LightComponent& other)
			: LightInstance(CreateRef<Light>(*other.LightInstance)),
			IconTexture(other.IconTexture) {
		}

		LightComponent(LightType type)
			: LightInstance(CreateRef<Light>(type)) {
			LoadIcon();
		}

		void LoadIcon() {
			IconTexture = Texture2D::Create("Resources/Icons/EntityIcons/LightIcon.png");
			if (!IconTexture) {
				LNX_LOG_WARN("Failed to load Light Icon");
			}
		}

		// Light properties accessors
		void SetType(LightType type) { LightInstance->SetType(type); }
		void SetColor(const glm::vec3& color) { LightInstance->SetColor(color); }
		void SetIntensity(float intensity) { LightInstance->SetIntensity(intensity); }
		void SetRange(float range) { LightInstance->SetRange(range); }
		void SetAttenuation(const glm::vec3& attenuation) { LightInstance->SetAttenuation(attenuation); }
		void SetInnerConeAngle(float angle) { LightInstance->SetInnerConeAngle(angle); }
		void SetOuterConeAngle(float angle) { LightInstance->SetOuterConeAngle(angle); }
		void SetCastShadows(bool cast) { LightInstance->SetCastShadows(cast); }

		LightType GetType() const { return LightInstance->GetType(); }
		const glm::vec3& GetColor() const { return LightInstance->GetColor(); }
		float GetIntensity() const { return LightInstance->GetIntensity(); }
		float GetRange() const { return LightInstance->GetRange(); }
		const glm::vec3& GetAttenuation() const { return LightInstance->GetAttenuation(); }
		float GetInnerConeAngle() const { return LightInstance->GetInnerConeAngle(); }
		float GetOuterConeAngle() const { return LightInstance->GetOuterConeAngle(); }
		bool GetCastShadows() const { return LightInstance->GetCastShadows(); }
	};

	struct CameraComponent {
		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	class ScriptableEntity;

	struct NativeScriptComponent {
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind() {
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	//physics
	struct Rigidbody2DComponent {
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	struct BoxCollider2DComponent {
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	struct CircleCollider2DComponent {
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 0.5f;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using AllComponents =
		ComponentGroup<TransformComponent, SpriteRendererComponent,
		CircleRendererComponent, CameraComponent, NativeScriptComponent,
		Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent,
		MeshComponent, MaterialComponent, LightComponent, TextureComponent>;
}