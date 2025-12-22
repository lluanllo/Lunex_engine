#pragma once

#include "Scene/Camera/SceneCamera.h"
#include "Core/UUID.h"
#include "Renderer/Texture.h"
#include "Resources/Mesh/Model.h"
#include "Scene/Lighting/Light.h"
#include "Renderer/EnvironmentMap.h"
#include "Log/Log.h"

// NEW MATERIAL SYSTEM - AAA Architecture
#include "Resources/Render/MaterialInstance.h"
#include "Assets/Materials/MaterialRegistry.h"

// NEW MESH ASSET SYSTEM - Unified Assets
#include "Assets/Mesh/MeshAsset.h"

// NEW CAMERA SYSTEM - AAA Architecture
#include "Scene/Camera/CameraData.h"

// NEW LIGHTING SYSTEM - AAA Architecture
#include "Scene/Lighting/LightTypes.h"

// NEW ANIMATION SYSTEM - AAA Architecture
#include "Scene/Components/AnimationComponents.h"

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
		
		// Get local transform matrix
		glm::mat4 GetLocalTransform() const {
			return GetTransform();
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

	// ========================================
	// MESH COMPONENT - Updated with MeshAsset support
	// ========================================
	struct MeshComponent {
		// Runtime model (loaded from primitive or MeshAsset)
		Ref<Model> MeshModel;
		
		// Mesh source type
		ModelType Type = ModelType::Cube;
		
		// ========== NEW: MeshAsset Support ==========
		// Reference to MeshAsset for custom models
		Ref<MeshAsset> Asset;
		UUID MeshAssetID;
		std::string MeshAssetPath;  // Path to .lumesh file
		
		// Legacy: Direct file path (deprecated, use MeshAsset instead)
		std::string FilePath;
		
		// Tint color
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		MeshComponent() {
			CreatePrimitive(ModelType::Cube);
		}
		
		MeshComponent(const MeshComponent& other)
			: MeshModel(other.MeshModel)
			, Type(other.Type)
			, Asset(other.Asset)
			, MeshAssetID(other.MeshAssetID)
			, MeshAssetPath(other.MeshAssetPath)
			, FilePath(other.FilePath)
			, Color(other.Color)
		{
		}
		
		MeshComponent(ModelType type)
			: Type(type) {
			CreatePrimitive(type);
		}

		// ========== PRIMITIVE CREATION ==========
		
		void CreatePrimitive(ModelType type) {
			Type = type;
			Asset = nullptr;
			MeshAssetID = UUID(0);
			MeshAssetPath.clear();
			FilePath.clear();
			
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
				// Will be loaded from Asset or FilePath
				MeshModel = nullptr;
				break;
			}
		}

		// ========== NEW: MeshAsset API ==========
		
		// Set mesh from a MeshAsset
		void SetMeshAsset(Ref<MeshAsset> meshAsset) {
			if (!meshAsset) return;
			
			Asset = meshAsset;
			MeshAssetID = meshAsset->GetID();
			MeshAssetPath = meshAsset->GetPath().string();
			Type = ModelType::FromFile;
			FilePath.clear();  // Clear legacy path
			
			// Get the model from the asset
			MeshModel = meshAsset->GetModel();
		}
		
		// Set mesh from a .lumesh file path
		void SetMeshAsset(const std::filesystem::path& assetPath) {
			if (!std::filesystem::exists(assetPath)) {
				LNX_LOG_WARN("MeshComponent::SetMeshAsset - File not found: {0}", assetPath.string());
				return;
			}
			
			// Check if it's a .lumesh file
			if (assetPath.extension() == ".lumesh") {
				Asset = MeshAsset::LoadFromFile(assetPath);
				if (Asset) {
					MeshAssetID = Asset->GetID();
					MeshAssetPath = assetPath.string();
					Type = ModelType::FromFile;
					FilePath.clear();
					MeshModel = Asset->GetModel();
				}
			}
			else {
				// Legacy: Direct model file - load directly
				LoadFromFile(assetPath.string());
			}
		}
		
		// Get the current MeshAsset
		Ref<MeshAsset> GetMeshAsset() const {
			return Asset;
		}
		
		// Clear the MeshAsset reference
		void ClearMeshAsset() {
			Asset = nullptr;
			MeshAssetID = UUID(0);
			MeshAssetPath.clear();
			MeshModel = nullptr;
			FilePath.clear();
		}
		
		// Check if using a MeshAsset
		bool HasMeshAsset() const {
			return Asset != nullptr;
		}
		
		// Get mesh metadata (from asset or calculated)
		uint32_t GetVertexCount() const {
			if (Asset) return Asset->GetVertexCount();
			if (!MeshModel) return 0;
			
			uint32_t count = 0;
			for (const auto& mesh : MeshModel->GetMeshes()) {
				count += static_cast<uint32_t>(mesh->GetVertices().size());
			}
			return count;
		}
		
		uint32_t GetTriangleCount() const {
			if (Asset) return Asset->GetTriangleCount();
			if (!MeshModel) return 0;
			
			uint32_t count = 0;
			for (const auto& mesh : MeshModel->GetMeshes()) {
				count += static_cast<uint32_t>(mesh->GetIndices().size()) / 3;
			}
			return count;
		}
		
		uint32_t GetSubmeshCount() const {
			if (Asset) return Asset->GetSubmeshCount();
			return MeshModel ? static_cast<uint32_t>(MeshModel->GetMeshes().size()) : 0;
		}

		// ========== LEGACY: Direct file loading (deprecated) ==========
		// Prefer using SetMeshAsset with a .lumesh file instead
		
		void LoadFromFile(const std::string& path) {
			FilePath = path;
			Type = ModelType::FromFile;
			Asset = nullptr;
			MeshAssetID = UUID(0);
			MeshAssetPath.clear();
			MeshModel = CreateRef<Model>(path);
		}
		
		// ========== UTILITY ==========
		
		// Reload mesh from source
		void Reload() {
			if (Asset) {
				Asset->ReloadModel();
				MeshModel = Asset->GetModel();
			}
			else if (!FilePath.empty()) {
				MeshModel = CreateRef<Model>(FilePath);
			}
		}
		
		// Check if mesh is valid and loaded
		bool IsValid() const {
			return MeshModel != nullptr && !MeshModel->GetMeshes().empty();
		}
	};

	struct MaterialComponent {
		// ========== NUEVA ARQUITECTURA ==========
		// Instancia del material (compartida o con overrides locales)
		Ref<MaterialInstance> Instance;
		
		// UUID del MaterialAsset (para serialización y lookup)
		UUID MaterialAssetID;
		
		// Path del asset (para UI y hot-reload)
		std::string MaterialAssetPath;
		
		// Preview thumbnail (generado por MaterialPreviewRenderer)
		Ref<Texture2D> PreviewThumbnail;
		
		// ========== CONSTRUCTORES ==========
		
		MaterialComponent() {
			// Usar material por defecto del registry
			auto defaultMaterial = MaterialRegistry::Get().GetDefaultMaterial();
			Instance = MaterialInstance::Create(defaultMaterial);
			MaterialAssetID = defaultMaterial->GetID();
			MaterialAssetPath = ""; // Material por defecto no tiene path
		}
		
		MaterialComponent(const MaterialComponent& other)
			: MaterialAssetID(other.MaterialAssetID)
			, MaterialAssetPath(other.MaterialAssetPath)
			, PreviewThumbnail(other.PreviewThumbnail)
		{
			// Clone the instance to preserve local overrides
			if (other.Instance) {
				Instance = other.Instance->Clone();
			}
			else {
				// Fallback to default material if source has no instance
				auto defaultMaterial = MaterialRegistry::Get().GetDefaultMaterial();
				Instance = MaterialInstance::Create(defaultMaterial);
			}
		}
		
		MaterialComponent& operator=(const MaterialComponent& other) {
			if (this != &other) {
				MaterialAssetID = other.MaterialAssetID;
				MaterialAssetPath = other.MaterialAssetPath;
				PreviewThumbnail = other.PreviewThumbnail;
				
				if (other.Instance) {
					Instance = other.Instance->Clone();
				}
				else {
					// Fallback to default material if source has no instance
					auto defaultMaterial = MaterialRegistry::Get().GetDefaultMaterial();
					Instance = MaterialInstance::Create(defaultMaterial);
				}
			}
			return *this;
		}
		
		MaterialComponent(Ref<MaterialAsset> asset) {
			Instance = MaterialInstance::Create(asset);
			MaterialAssetID = asset->GetID();
			MaterialAssetPath = asset->GetPath().string();
		}
		
		MaterialComponent(const std::filesystem::path& assetPath) {
			auto asset = MaterialRegistry::Get().LoadMaterial(assetPath);
			if (asset) {
				Instance = MaterialInstance::Create(asset);
				MaterialAssetID = asset->GetID();
				MaterialAssetPath = assetPath.string();
			} else {
				// Fallback a material por defecto
				auto defaultMaterial = MaterialRegistry::Get().GetDefaultMaterial();
				Instance = MaterialInstance::Create(defaultMaterial);
				MaterialAssetID = defaultMaterial->GetID();
				MaterialAssetPath = "";
			}
		}
		
		// ========== API DE MATERIAL ==========
		
		// Cambiar el MaterialAsset base (perderá overrides locales)
		void SetMaterialAsset(Ref<MaterialAsset> asset) {
			if (!asset) return;
			
			Instance->SetBaseAsset(asset);
			MaterialAssetID = asset->GetID();
			MaterialAssetPath = asset->GetPath().string();
		}
		
		void SetMaterialAsset(const std::filesystem::path& assetPath) {
			auto asset = MaterialRegistry::Get().LoadMaterial(assetPath);
			if (asset) {
				SetMaterialAsset(asset);
			}
		}
		
		// Obtener información del material
		std::string GetMaterialName() const {
			return Instance ? Instance->GetName() : "None";
		}
		
		UUID GetAssetID() const {
			return MaterialAssetID;
		}
		
		const std::string& GetAssetPath() const {
			return MaterialAssetPath;
		}
		
		Ref<MaterialAsset> GetBaseAsset() const {
			return Instance ? Instance->GetBaseAsset() : nullptr;
		}
		
		// Verificar si hay overrides locales
		bool HasLocalOverrides() const {
			return Instance ? Instance->HasLocalOverrides() : false;
		}
		
		// Resetear overrides (volver al asset base)
		void ResetOverrides() {
			if (Instance) {
				Instance->ResetOverrides();
			}
		}
		
		// ========== PROPERTIES ACCESSORS (con override support) ==========
		// Nota: `asOverride = true` para modificar solo esta instancia
		//       `asOverride = false` para modificar el asset base (afecta a todos)
		
		void SetAlbedo(const glm::vec4& color, bool asOverride = true) {
			if (Instance) Instance->SetAlbedo(color, asOverride);
		}
		
		glm::vec4 GetAlbedo() const {
			return Instance ? Instance->GetAlbedo() : glm::vec4(1.0f);
		}
		
		void SetMetallic(float metallic, bool asOverride = true) {
			if (Instance) Instance->SetMetallic(metallic, asOverride);
		}
		
		float GetMetallic() const {
			return Instance ? Instance->GetMetallic() : 0.0f;
		}
		
		void SetRoughness(float roughness, bool asOverride = true) {
			if (Instance) Instance->SetRoughness(roughness, asOverride);
		}
		
		float GetRoughness() const {
			return Instance ? Instance->GetRoughness() : 0.5f;
		}
		
		void SetSpecular(float specular, bool asOverride = true) {
			if (Instance) Instance->SetSpecular(specular, asOverride);
		}
		
		float GetSpecular() const {
			return Instance ? Instance->GetSpecular() : 0.5f;
		}
		
		void SetEmissionColor(const glm::vec3& color, bool asOverride = true) {
			if (Instance) Instance->SetEmissionColor(color, asOverride);
		}
		
		glm::vec3 GetEmissionColor() const {
			return Instance ? Instance->GetEmissionColor() : glm::vec3(0.0f);
		}
		
		void SetEmissionIntensity(float intensity, bool asOverride = true) {
			if (Instance) Instance->SetEmissionIntensity(intensity, asOverride);
		}
		
		float GetEmissionIntensity() const {
			return Instance ? Instance->GetEmissionIntensity() : 0.0f;
		}
		
		// ========== LEGACY API (para compatibilidad temporal) ==========
		// Deprecated: usar SetAlbedo/GetAlbedo en su lugar
		void SetColor(const glm::vec4& color) { SetAlbedo(color); }
		const glm::vec4 GetColor() const { return GetAlbedo(); }
	};
	
	// ========================================
	// DEPRECATED: TextureComponent
	// ========================================
	// ?? Este componente está OBSOLETO a partir de la nueva arquitectura de materiales
	// 
	// ANTES (sistema antiguo):
	//   MaterialComponent - propiedades PBR
	//   TextureComponent  - texturas PBR
	//
	// AHORA (sistema nuevo):
	//   MaterialComponent - contiene MaterialInstance que incluye TODO:
	//     - Propiedades PBR (metallic, roughness, etc.)
	//     - Texturas PBR (albedo, normal, metallic, etc.)
	//     - Multipliers y configuración avanzada
	//
	// MIGRACIÓN:
	//   1. Crear o cargar un MaterialAsset (.lumat)
	//   2. Asignar texturas al MaterialAsset
	//   3. Asignar el MaterialAsset al MaterialComponent
	//   4. Eliminar TextureComponent de la entidad
	//
	// Este componente se mantendrá temporalmente para compatibilidad con escenas antiguas,
	// pero será eliminado en una versión futura.
	// ========================================
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
			if (!IconTexture || !IconTexture->IsLoaded()) {
				LNX_LOG_ERROR("Failed to load Light Icon from Resources/Icons/EntityIcons/LightIcon.png");
				LNX_LOG_ERROR("  -> Check that the file exists at this path");
			}
			else {
				LNX_LOG_INFO("Light Icon loaded successfully: {0}x{1}", IconTexture->GetWidth(), IconTexture->GetHeight());
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
		Ref<Texture2D> IconTexture;

		CameraComponent() {
			LoadIcon();
		}
		
		CameraComponent(const CameraComponent& other)
			: Camera(other.Camera),
			  Primary(other.Primary),
			  FixedAspectRatio(other.FixedAspectRatio),
			  IconTexture(other.IconTexture) {
		}

		void LoadIcon() {
			IconTexture = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
			if (!IconTexture || !IconTexture->IsLoaded()) {
				LNX_LOG_ERROR("Failed to load Camera Icon from Resources/Icons/HierarchyPanel/CameraIcon.png");
				LNX_LOG_ERROR("  -> Check that the file exists at this path");
			}
			else {
				LNX_LOG_INFO("Camera Icon loaded successfully: {0}x{1}", IconTexture->GetWidth(), IconTexture->GetHeight());
			}
		}
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

	// ========================================
	// 3D PHYSICS COMPONENTS (Bullet3)
	// ========================================

	struct Rigidbody3DComponent {
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Dynamic;
		
		// Physics properties
		float Mass = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.3f;
		float LinearDamping = 0.04f;
		float AngularDamping = 0.05f;
		
		// Constraints
		glm::vec3 LinearFactor = { 1.0f, 1.0f, 1.0f };  // Lock axes (0 = locked, 1 = free)
		glm::vec3 AngularFactor = { 1.0f, 1.0f, 1.0f }; // Lock rotation axes
		
		// CCD (Continuous Collision Detection) for fast moving objects
		bool UseCCD = false;
		float CcdMotionThreshold = 0.0f;
		float CcdSweptSphereRadius = 0.0f;
		
		// Collision filtering
		bool IsTrigger = false;
		int CollisionGroup = 1;
		int CollisionMask = -1; // All groups by default
		
		// Runtime data (not serialized)
		void* RuntimeBody = nullptr;
		void* RuntimeCollider = nullptr;
		
		Rigidbody3DComponent() = default;
		Rigidbody3DComponent(const Rigidbody3DComponent&) = default;
	};

	struct BoxCollider3DComponent {
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		
		BoxCollider3DComponent() = default;
		BoxCollider3DComponent(const BoxCollider3DComponent&) = default;
	};

	struct SphereCollider3DComponent {
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		
		SphereCollider3DComponent() = default;
		SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
	};

	struct CapsuleCollider3DComponent {
		float Radius = 0.5f;
		float Height = 2.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		
		CapsuleCollider3DComponent() = default;
		CapsuleCollider3DComponent(const CapsuleCollider3DComponent&) = default;
	};

	struct MeshCollider3DComponent {
		enum class CollisionType { Convex, Concave };
		CollisionType Type = CollisionType::Convex;
		
		// For custom meshes
		std::vector<glm::vec3> Vertices;
		std::vector<uint32_t> Indices;
		
		// Will use entity's MeshComponent if available
		bool UseEntityMesh = true;
		
		MeshCollider3DComponent() = default;
		MeshCollider3DComponent(const MeshCollider3DComponent&) = default;
	};

	// C++ Script Component - integración con sistema de scripting dinámico
	// SOPORTA MÚLTIPLES SCRIPTS POR ENTIDAD
	struct ScriptComponent {
		// Lista de scripts asociados a esta entidad
		std::vector<std::string> ScriptPaths;          // Rutas relativas a scripts .cpp
		std::vector<std::string> CompiledDLLPaths;     // Rutas a DLLs compiladas
		std::vector<bool> ScriptLoadedStates;          // Estado de carga de cada script
		
		bool AutoCompile = true;         // Auto-compilar al entrar en Play mode
		
		// Runtime data (no serializar) - un plugin por script
		std::vector<void*> ScriptPluginInstances;  // Punteros a ScriptPlugin*
		
		ScriptComponent() = default;
		
		ScriptComponent(const ScriptComponent& other)
			: ScriptPaths(other.ScriptPaths)
			, CompiledDLLPaths(other.CompiledDLLPaths)
			, ScriptLoadedStates(other.ScriptPaths.size(), false)
			, AutoCompile(other.AutoCompile)
		{
		}
		
		ScriptComponent(const std::string& scriptPath)
			: AutoCompile(true)
		{
			AddScript(scriptPath);
		}
		
		~ScriptComponent() {
			// Cleanup manejado por Scene
		}
		
		// ===== API PARA GESTIÓN DE SCRIPTS =====
		
		void AddScript(const std::string& scriptPath) {
			ScriptPaths.push_back(scriptPath);
			CompiledDLLPaths.push_back("");
			ScriptLoadedStates.push_back(false);
			ScriptPluginInstances.push_back(nullptr);
		}
		
		void RemoveScript(size_t index) {
			if (index < ScriptPaths.size()) {
				ScriptPaths.erase(ScriptPaths.begin() + index);
				CompiledDLLPaths.erase(CompiledDLLPaths.begin() + index);
				ScriptLoadedStates.erase(ScriptLoadedStates.begin() + index);
				ScriptPluginInstances.erase(ScriptPluginInstances.begin() + index);
			}
		}
		
		size_t GetScriptCount() const {
			return ScriptPaths.size();
		}
		
		bool IsScriptLoaded(size_t index) const {
			return index < ScriptLoadedStates.size() && ScriptLoadedStates[index];
		}
		
		const std::string& GetScriptPath(size_t index) const {
			static std::string empty;
			return (index < ScriptPaths.size()) ? ScriptPaths[index] : empty;
		}
		
		// Compatibilidad con código legacy (usa el primer script)
		std::string GetLegacyScriptPath() const {
			return ScriptPaths.empty() ? "" : ScriptPaths[0];
		}
		
		void SetLegacyScriptPath(const std::string& path) {
			if (ScriptPaths.empty()) {
				AddScript(path);
			} else {
				ScriptPaths[0] = path;
				ScriptLoadedStates[0] = false;
			}
		}
	};

	// ========================================
	// RELATIONSHIP COMPONENT (Parent-Child Hierarchy)
	// ========================================
	struct RelationshipComponent {
		UUID ParentID = 0;                    // UUID del padre (0 = sin padre, root)
		std::vector<UUID> ChildrenIDs;        // UUIDs de los hijos
		
		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		
		bool HasParent() const { return ParentID != 0; }
		bool HasChildren() const { return !ChildrenIDs.empty(); }
		size_t GetChildCount() const { return ChildrenIDs.size(); }
		
		void AddChild(UUID childID) {
			// Evitar duplicados
			for (const auto& id : ChildrenIDs) {
				if (id == childID) return;
			}
			ChildrenIDs.push_back(childID);
		}
		
		void RemoveChild(UUID childID) {
			ChildrenIDs.erase(
				std::remove(ChildrenIDs.begin(), ChildrenIDs.end(), childID),
				ChildrenIDs.end()
			);
		}
		
		void ClearChildren() {
			ChildrenIDs.clear();
		}
		
		void SetParent(UUID parentID) {
			ParentID = parentID;
		}
		
		void ClearParent() {
			ParentID = 0;
		}
	};

	// ========================================
	// ENVIRONMENT COMPONENT (Skybox/HDRI/IBL)
	// ========================================
	// 
	// Provides Image-Based Lighting (IBL) for the scene.
	// Only ONE EnvironmentComponent should be active per scene.
	// If multiple exist, the first one found will be used.
	// ========================================
	struct EnvironmentComponent {
		// The environment map (contains cubemap, irradiance, prefiltered maps)
		Ref<EnvironmentMap> Environment;
		
		// Path to the HDRI file (for serialization)
		std::string HDRIPath;
		
		// Is this the active environment? (only one can be active)
		bool IsActive = true;
		
		// Render as skybox background?
		bool RenderSkybox = true;
		
		// Use for IBL lighting?
		bool UseForLighting = true;
		
		// Resolution for cubemap (higher = better quality, more VRAM)
		uint32_t CubemapResolution = 1024;
		
		EnvironmentComponent() {
			Environment = CreateRef<EnvironmentMap>();
		}
		
		EnvironmentComponent(const EnvironmentComponent& other)
			: Environment(other.Environment)
			, HDRIPath(other.HDRIPath)
			, IsActive(other.IsActive)
			, RenderSkybox(other.RenderSkybox)
			, UseForLighting(other.UseForLighting)
			, CubemapResolution(other.CubemapResolution)
		{
		}
		
		EnvironmentComponent(const std::string& hdriPath, uint32_t resolution = 1024)
			: HDRIPath(hdriPath)
			, CubemapResolution(resolution)
		{
			Environment = CreateRef<EnvironmentMap>();
			if (!hdriPath.empty()) {
				LoadHDRI(hdriPath);
			}
		}
		
		// ========== LOADING ==========
		
		bool LoadHDRI(const std::string& path) {
			HDRIPath = path;
			if (!Environment) {
				Environment = CreateRef<EnvironmentMap>();
			}
			bool success = Environment->LoadFromHDRI(path, CubemapResolution);
			if (success) {
				LNX_LOG_INFO("Environment loaded: {0}", path);
			} else {
				LNX_LOG_ERROR("Failed to load environment: {0}", path);
			}
			return success;
		}
		
		bool LoadFromFaces(const std::array<std::string, 6>& facePaths) {
			if (!Environment) {
				Environment = CreateRef<EnvironmentMap>();
			}
			bool success = Environment->LoadFromFaces(facePaths);
			if (success) {
				HDRIPath = facePaths[0]; // Use first face as identifier
			}
			return success;
		}
		
		// ========== SETTINGS ==========
		
		void SetIntensity(float intensity) {
			if (Environment) Environment->SetIntensity(intensity);
		}
		
		float GetIntensity() const {
			return Environment ? Environment->GetIntensity() : 1.0f;
		}
		
		void SetRotation(float rotationDegrees) {
			if (Environment) Environment->SetRotation(glm::radians(rotationDegrees));
		}
		
		float GetRotation() const {
			return Environment ? glm::degrees(Environment->GetRotation()) : 0.0f;
		}
		
		void SetTint(const glm::vec3& tint) {
			if (Environment) Environment->SetTint(tint);
		}
		
		glm::vec3 GetTint() const {
			return Environment ? Environment->GetTint() : glm::vec3(1.0f);
		}
		
		void SetBlur(float blur) {
			if (Environment) Environment->SetBlur(blur);
		}
		
		float GetBlur() const {
			return Environment ? Environment->GetBlur() : 0.0f;
		}
		
		// ========== STATE ==========
		
		bool IsLoaded() const {
			return Environment && Environment->IsLoaded();
		}
		
		Ref<TextureCube> GetEnvironmentMap() const {
			return Environment ? Environment->GetEnvironmentMap() : nullptr;
		}
		
		Ref<TextureCube> GetIrradianceMap() const {
			return Environment ? Environment->GetIrradianceMap() : nullptr;
		}
		
		Ref<TextureCube> GetPrefilteredMap() const {
			return Environment ? Environment->GetPrefilteredMap() : nullptr;
		}
	};

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using AllComponents =
		ComponentGroup<TransformComponent, SpriteRendererComponent,
		CircleRendererComponent, CameraComponent, NativeScriptComponent,
		Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent,
		Rigidbody3DComponent, BoxCollider3DComponent, SphereCollider3DComponent, 
		CapsuleCollider3DComponent, MeshCollider3DComponent,
		MeshComponent, MaterialComponent, LightComponent, TextureComponent, ScriptComponent,
		RelationshipComponent, EnvironmentComponent,
		SkeletalMeshComponent, AnimatorComponent>;  // ? Added Animation Components
}