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

// Material importer for auto-generating materials from models
#include "Assets/Materials/MaterialImporter.h"

// NEW CAMERA SYSTEM - AAA Architecture
#include "Scene/Camera/CameraData.h"

// NEW LIGHTING SYSTEM - AAA Architecture
#include "Scene/Lighting/LightTypes.h"
#include "Scripting/ScriptReflection.h"
#include <unordered_map>

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

		// Imported material assets (auto-generated from model, one per unique material)
		std::vector<Ref<MaterialAsset>> ImportedMaterials;
		
		// Material output directory for auto-generated .lumat files
		std::string MaterialOutputDir;

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
			, ImportedMaterials(other.ImportedMaterials)
			, MaterialOutputDir(other.MaterialOutputDir)
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
			
			// Auto-generate materials from model data
			AutoImportMaterials();
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
					
					// Auto-generate materials from model data
					AutoImportMaterials();
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
			
			// Auto-generate materials from model data
			AutoImportMaterials();
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
			
			// Re-import materials after reload
			AutoImportMaterials();
		}
		
		// Check if mesh is valid and loaded
		bool IsValid() const {
			return MeshModel != nullptr && !MeshModel->GetMeshes().empty();
		}
		
		// ========== MATERIAL AUTO-IMPORT ==========
		
		// Get the material output directory for auto-generated .lumat files
		std::filesystem::path GetMaterialOutputDirectory() const {
			if (!MaterialOutputDir.empty()) {
				return MaterialOutputDir;
			}
			
			// Default: create materials directory next to the model file
			std::string sourcePath;
			if (Asset && Asset->HasValidSource()) {
				sourcePath = Asset->GetSourcePath().string();
			} else if (!FilePath.empty()) {
				sourcePath = FilePath;
			}
			
			if (!sourcePath.empty()) {
				std::filesystem::path modelDir = std::filesystem::path(sourcePath).parent_path();
				return modelDir / "Materials";
			}
			
			// Fallback to assets/Materials
			return "assets/Materials";
		}
		
		// Auto-import materials from model data and generate .lumat files
		void AutoImportMaterials() {
			if (!MeshModel || !MeshModel->HasMaterialData()) {
				return;
			}
			
			std::filesystem::path outputDir = GetMaterialOutputDirectory();
			
			LNX_LOG_INFO("MeshComponent: Auto-importing materials to {0}", outputDir.string());
			
			ImportedMaterials = MaterialImporter::CreateMaterialsFromModel(MeshModel, outputDir);
			
			if (!ImportedMaterials.empty()) {
				LNX_LOG_INFO("MeshComponent: Imported {0} materials from model", ImportedMaterials.size());
			}
		}
		
		// Get the MaterialAsset for a specific submesh index
		Ref<MaterialAsset> GetSubmeshMaterial(uint32_t submeshIndex) const {
			if (ImportedMaterials.empty() || !MeshModel) {
				return nullptr;
			}
			
			const auto& indices = MeshModel->GetSubmeshMaterialIndices();
			if (submeshIndex < indices.size()) {
				uint32_t matIdx = indices[submeshIndex];
				if (matIdx < ImportedMaterials.size()) {
					return ImportedMaterials[matIdx];
				}
			}
			
			// Fallback: return first material if available
			return ImportedMaterials.empty() ? nullptr : ImportedMaterials[0];
		}
		
		// Check if model has per-submesh materials
		bool HasPerSubmeshMaterials() const {
			return !ImportedMaterials.empty() && MeshModel && 
				   !MeshModel->GetSubmeshMaterialIndices().empty();
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
		
		// True while the entity still uses the registry's default material: imported
		// per-submesh materials are used then; an assigned material overrides them all.
		bool IsDefaultMaterial() const {
			return !Instance || !Instance->GetBaseAsset() ||
				Instance->GetBaseAsset() == MaterialRegistry::Get().GetDefaultMaterial();
		}

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
		
		void SetNormalIntensity(float intensity, bool asOverride = true) {
			if (Instance) Instance->SetNormalIntensity(intensity, asOverride);
		}
		
		float GetNormalIntensity() const {
			return Instance ? Instance->GetNormalIntensity() : 1.0f;
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

		// ========================================
		// BASIC LIGHT PROPERTIES
		// ========================================
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
		
		// ========================================
		// SUN/SKY SETTINGS (Directional Light only)
		// ========================================
		
		// Mark this light as the "Sun" that controls the skybox
		void SetIsSunLight(bool isSun) { LightInstance->SetIsSunLight(isSun); }
		bool IsSunLight() const { return LightInstance->IsSunLight(); }
		
		// Link to skybox rotation
		void SetLinkToSkyboxRotation(bool link) { LightInstance->SetLinkToSkyboxRotation(link); }
		bool GetLinkToSkyboxRotation() const { return LightInstance->GetLinkToSkyboxRotation(); }
		
		// Skybox intensity multiplier
		void SetSkyboxIntensityMultiplier(float mult) { LightInstance->SetSkyboxIntensityMultiplier(mult); }
		float GetSkyboxIntensityMultiplier() const { return LightInstance->GetSkyboxIntensityMultiplier(); }
		
		// Atmosphere settings
		void SetAffectAtmosphere(bool affect) { LightInstance->SetAffectAtmosphere(affect); }
		bool GetAffectAtmosphere() const { return LightInstance->GetAffectAtmosphere(); }
		
		void SetAtmosphericDensity(float density) { LightInstance->SetAtmosphericDensity(density); }
		float GetAtmosphericDensity() const { return LightInstance->GetAtmosphericDensity(); }
		
		// Sun disk appearance (for procedural sky)
		void SetRenderSunDisk(bool render) { LightInstance->SetRenderSunDisk(render); }
		bool GetRenderSunDisk() const { return LightInstance->GetRenderSunDisk(); }
		
		void SetSunDiskSize(float size) { LightInstance->SetSunDiskSize(size); }
		float GetSunDiskSize() const { return LightInstance->GetSunDiskSize(); }
		
		void SetSunDiskIntensity(float intensity) { LightInstance->SetSunDiskIntensity(intensity); }
		float GetSunDiskIntensity() const { return LightInstance->GetSunDiskIntensity(); }
		
		// Ambient contribution
		void SetContributeToAmbient(bool contribute) { LightInstance->SetContributeToAmbient(contribute); }
		bool GetContributeToAmbient() const { return LightInstance->GetContributeToAmbient(); }
		
		void SetAmbientContribution(float contribution) { LightInstance->SetAmbientContribution(contribution); }
		float GetAmbientContribution() const { return LightInstance->GetAmbientContribution(); }
		
		void SetGroundColor(const glm::vec3& color) { LightInstance->SetGroundColor(color); }
		const glm::vec3& GetGroundColor() const { return LightInstance->GetGroundColor(); }
		
		// Time of day
		void SetUseTimeOfDay(bool use) { LightInstance->SetUseTimeOfDay(use); }
		bool GetUseTimeOfDay() const { return LightInstance->GetUseTimeOfDay(); }
		
		void SetTimeOfDay(float time) { LightInstance->SetTimeOfDay(time); }
		float GetTimeOfDay() const { return LightInstance->GetTimeOfDay(); }
		
		void SetTimeOfDaySpeed(float speed) { LightInstance->SetTimeOfDaySpeed(speed); }
		float GetTimeOfDaySpeed() const { return LightInstance->GetTimeOfDaySpeed(); }
		
		// Get full Sun/Sky settings
		const SunSkySettings& GetSunSkySettings() const { return LightInstance->GetSunSkySettings(); }
		SunSkySettings& GetSunSkySettings() { return LightInstance->GetSunSkySettings(); }
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

	// ========================================================================
	// GROUP (Blender-style collection / folder in the hierarchy)
	// ========================================================================

	/**
	 * @struct GroupComponent
	 * @brief Marks an entity as a folder-like group. Children are regular parented
	 *        entities; the group keeps an identity transform so it never moves them.
	 */
	struct GroupComponent {
		bool Expanded = true;
		GroupComponent() = default;
		GroupComponent(const GroupComponent&) = default;
	};

	// ========================================================================
	// ANIMATION
	// ========================================================================

	/**
	 * @struct AnimatorComponent
	 * @brief Plays skeletal animation clips of the entity's (skinned) MeshComponent model
	 */
	struct AnimatorComponent {
		int   ClipIndex = 0;            // index into Model::GetAnimations()
		float Speed = 1.0f;
		bool  Loop = true;
		bool  PlayOnStart = true;
		bool  PreviewInEditor = false;  // advance time in Edit mode too
		float BlendDuration = 0.25f;    // crossfade time when switching clips (seconds)

		// Editor debug views
		bool  ShowSkeleton = false;     // draw bones in the viewport
		int   DebugWeightsBone = -1;    // >= 0: show that bone's vertex weights as a heatmap

		// Authored clips (.luanim, paths relative to the asset dir or absolute). They are
		// listed after the model's own clips: ClipIndex >= modelClipCount selects one.
		std::vector<std::string> AnimationAssets;

		// Runtime IK: after sampling, each chain bends so EndBone reaches the target entity
		struct IKChain {
			std::string EndBone;        // e.g. "mixamorig:LeftHand"
			int ChainLength = 2;        // joints above the end bone (2 = arm/leg)
			uint64_t TargetEntity = 0;  // UUID of the entity to reach
			float Weight = 1.0f;
			bool Enabled = true;
		};
		std::vector<IKChain> IKChains;

		// Animation editor: when set, this pose is shown instead of the sampled clip
		bool UseEditPose = false;
		std::vector<NodeTRS> EditPose;
		int EditorSelectedNode = -1;    // bone highlighted in the skeleton overlay (not serialized)

		// Runtime (not serialized)
		float Time = 0.0f;
		bool  Playing = false;
		int   PreviousClip = -1;        // clip fading out (crossfade), -1 = none
		float PreviousTime = 0.0f;
		float BlendElapsed = 0.0f;
		std::vector<glm::mat4> BoneMatrices;   // skinning palette for this frame
		std::vector<glm::mat4> NodeTransforms; // model-space node transforms (skeleton view)

		// Start a clip from the beginning (crossfades from the current one if it was playing)
		void Play(int clip = -1) {
			if (clip >= 0 && clip != ClipIndex && Playing && BlendDuration > 0.0f) {
				PreviousClip = ClipIndex;
				PreviousTime = Time;
				BlendElapsed = 0.0f;
			}
			if (clip >= 0) ClipIndex = clip;
			Time = 0.0f;
			Playing = true;
		}
		void Stop() { Playing = false; Time = 0.0f; PreviousClip = -1; }

		AnimatorComponent() = default;
		AnimatorComponent(const AnimatorComponent&) = default;
	};

	//physics
	struct Rigidbody2DComponent {
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		// See Rigidbody3DComponent::LastSynced*
		glm::vec2 LastSyncedTranslation = { 0.0f, 0.0f };
		float LastSyncedRotation = 0.0f;

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

		// Transform last exchanged with the physics world. If the TransformComponent differs
		// at the start of a step, something else (a script, the gizmo) moved the entity and
		// the change is pushed into the physics body instead of being overwritten.
		glm::vec3 LastSyncedTranslation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 LastSyncedRotation = { 0.0f, 0.0f, 0.0f };

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

	struct CylinderCollider3DComponent {
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f }; // x=radius, y=halfHeight, z=radius
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		
		CylinderCollider3DComponent() = default;
		CylinderCollider3DComponent(const CylinderCollider3DComponent&) = default;
	};

	struct ConeCollider3DComponent {
		float Radius = 0.5f;
		float Height = 1.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		
		ConeCollider3DComponent() = default;
		ConeCollider3DComponent(const ConeCollider3DComponent&) = default;
	};

	struct CharacterController3DComponent {
		// Capsule shape for the character
		float Radius = 0.3f;
		float Height = 1.8f;
		
		// Movement properties
		float StepHeight = 0.35f;       // Max step the character can climb
		float MaxSlopeAngle = 45.0f;    // Max slope angle in degrees the character can walk
		float MoveSpeed = 5.0f;         // Default movement speed (m/s)
		float JumpForce = 5.0f;         // Initial jump velocity (m/s)
		float GravityScale = 1.0f;      // Multiplier for gravity
		
		// Ground detection
		float SkinWidth = 0.08f;        // Small offset to prevent getting stuck
		bool IsGrounded = false;        // Runtime: is the character on the ground
		
		// Runtime data (not serialized)
		void* RuntimeController = nullptr;
		void* RuntimeGhostObject = nullptr;
		
		CharacterController3DComponent() = default;
		CharacterController3DComponent(const CharacterController3DComponent&) = default;
	};

	// C++ Script Component - integración con sistema de scripting dinámico
	// SOPORTA MÚLTIPLES SCRIPTS POR ENTIDAD
	struct ScriptComponent {
		// Lista de scripts asociados a esta entidad
		std::vector<std::string> ScriptPaths;          // Rutas relativas a scripts .cpp
		std::vector<std::string> CompiledDLLPaths;     // Rutas a DLLs compiladas
		std::vector<bool> ScriptLoadedStates;          // Estado de carga de cada script
		
		bool AutoCompile = true;         // Auto-compilar al entrar en Play mode

		
		// Public variable values edited in the inspector (one map per script, name -> value)
		std::vector<std::unordered_map<std::string, ScriptFieldValue>> FieldOverrides;

		// Runtime data (no serializar) - un plugin por script
		std::vector<void*> ScriptPluginInstances;  // Punteros a ScriptPlugin*
		
		ScriptComponent() = default;
		
		ScriptComponent(const ScriptComponent& other)
			: ScriptPaths(other.ScriptPaths)
			, CompiledDLLPaths(other.CompiledDLLPaths)
			, ScriptLoadedStates(other.ScriptPaths.size(), false)
			, AutoCompile(other.AutoCompile)
			, FieldOverrides(other.FieldOverrides)
			, ScriptPluginInstances(other.ScriptPaths.size(), nullptr)
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
			FieldOverrides.emplace_back();
		}
		
		void RemoveScript(size_t index) {
			if (index < ScriptPaths.size()) {
				ScriptPaths.erase(ScriptPaths.begin() + index);
				CompiledDLLPaths.erase(CompiledDLLPaths.begin() + index);
				ScriptLoadedStates.erase(ScriptLoadedStates.begin() + index);
				ScriptPluginInstances.erase(ScriptPluginInstances.begin() + index);
				if (index < FieldOverrides.size())
					FieldOverrides.erase(FieldOverrides.begin() + index);
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

	// ========================================================================
	// BLUEPRINT (visual scripting)
	// ========================================================================

	// Runs a .lublueprint node graph on this entity in Play mode (BlueprintSystem)
	struct BlueprintComponent {
		std::string BlueprintPath;                 // relative to the asset directory when possible
		std::shared_ptr<void> Runtime;             // Blueprint::Instance (not serialized, not copied)

		BlueprintComponent() = default;
		BlueprintComponent(const BlueprintComponent& other) : BlueprintPath(other.BlueprintPath) {}
		BlueprintComponent& operator=(const BlueprintComponent& other) { BlueprintPath = other.BlueprintPath; Runtime.reset(); return *this; }
	};

	// ========================================================================
	// PARTICLE SYSTEM
	// ========================================================================

	/**
	 * Emitter of particles. A particle can be a camera-facing sprite, a .lumesh model
	 * (with any material, including node-graph materials) or a .luprefab instance.
	 *
	 * Simulation:
	 *  - Simple:  CPU integration (gravity, drag, ground bounce). Cheap, thousands of particles.
	 *  - Physics: every particle is a real entity with Rigidbody3D + collider simulated by
	 *             Bullet (prefabs keep their own physics components). Use for debris, splashes...
	 * Scripts/blueprints drive it with Emit / Play / Stop / SetRate.
	 */
	struct ParticleSystemComponent {
		enum class EmitterShape { Point = 0, Sphere, Box, Cone };
		enum class RenderMode { Billboard = 0, Mesh, Prefab };
		enum class SimulationMode { Simple = 0, Physics };

		// ---- Emission ----
		bool PlayOnStart = true;
		bool PreviewInEditor = true;     // simulate in Edit mode (Simple + Billboard/Mesh only)
		bool Looping = true;
		float Duration = 5.0f;           // seconds per cycle (when not looping, emission stops after it)
		float Rate = 20.0f;              // particles per second
		int Burst = 0;                   // particles emitted at once when the cycle starts
		int MaxParticles = 500;
		bool WorldSpace = true;          // particles stay where they were born when the emitter moves

		// ---- Shape ----
		EmitterShape Shape = EmitterShape::Cone;
		float ShapeRadius = 0.25f;
		glm::vec3 BoxSize{ 1.0f };
		float ConeAngle = 25.0f;         // degrees around the emitter's +Y

		// ---- Particle (min/max ranges) ----
		glm::vec2 Lifetime{ 1.5f, 2.5f };
		glm::vec2 Speed{ 2.0f, 4.0f };
		glm::vec2 StartSize{ 0.15f, 0.3f };
		float EndSizeScale = 0.2f;       // size multiplier at the end of the life
		glm::vec2 AngularSpeed{ -90.0f, 90.0f };   // degrees/second
		glm::vec4 StartColor{ 1.0f, 0.9f, 0.6f, 1.0f };
		glm::vec4 EndColor{ 1.0f, 0.3f, 0.1f, 0.0f };

		// ---- Forces ----
		float GravityScale = 0.3f;       // x world gravity (-9.81 Y)
		float Drag = 0.2f;
		bool CollideWithGround = false;
		float GroundHeight = 0.0f;
		float Bounce = 0.4f;

		// ---- Rendering ----
		RenderMode Mode = RenderMode::Billboard;
		SimulationMode Simulation = SimulationMode::Simple;
		std::string TexturePath;         // billboard sprite (empty = soft disc)
		std::string MeshPath;            // .lumesh for Mesh mode
		std::string MaterialPath;        // .lumat for Mesh mode (empty = default material)
		std::string PrefabPath;          // .luprefab for Prefab mode
		bool AdditiveBlend = false;
		float ParticleMass = 0.2f;       // Physics mode

		// ---- Runtime (not serialized) ----
		struct Particle {
			glm::vec3 Position{ 0.0f };
			glm::vec3 Velocity{ 0.0f };
			float Age = 0.0f;
			float Life = 1.0f;
			float Size = 0.2f;
			float Rotation = 0.0f;
			float AngularSpeed = 0.0f;
			glm::vec4 Color{ 1.0f };
			uint64_t Entity = 0;         // spawned entity UUID (Physics / Prefab modes)
		};
		std::vector<Particle> Particles;
		bool Playing = false;
		float CycleTime = 0.0f;
		float EmitAccumulator = 0.0f;
		bool BurstPending = true;
		int PendingEmit = 0;             // Emit(n) requests from scripts/blueprints
		uint32_t RandomState = 0x9E3779B9u;
		std::string LoadedMeshPath, LoadedMaterialPath, LoadedTexturePath, LoadedPrefabPath;
		Ref<Model> RuntimeMesh;
		Ref<MaterialInstance> RuntimeMaterial;
		Ref<Texture2D> RuntimeTexture;
		Ref<void> RuntimePrefab;         // Ref<Prefab> (kept opaque to avoid a dependency here)

		void Play() { Playing = true; CycleTime = 0.0f; BurstPending = true; }
		void Stop() { Playing = false; }
		void Emit(int count) { PendingEmit += std::max(count, 0); }
		void Clear() { Particles.clear(); }

		ParticleSystemComponent() = default;
		ParticleSystemComponent(const ParticleSystemComponent& other) { *this = other; }
		ParticleSystemComponent& operator=(const ParticleSystemComponent& other) {
			if (this == &other) return *this;
			// Copy settings only: a copy (Play mode scene, duplicate) starts with no live particles
			PlayOnStart = other.PlayOnStart; PreviewInEditor = other.PreviewInEditor; Looping = other.Looping;
			Duration = other.Duration; Rate = other.Rate; Burst = other.Burst; MaxParticles = other.MaxParticles;
			WorldSpace = other.WorldSpace; Shape = other.Shape; ShapeRadius = other.ShapeRadius; BoxSize = other.BoxSize;
			ConeAngle = other.ConeAngle; Lifetime = other.Lifetime; Speed = other.Speed; StartSize = other.StartSize;
			EndSizeScale = other.EndSizeScale; AngularSpeed = other.AngularSpeed; StartColor = other.StartColor;
			EndColor = other.EndColor; GravityScale = other.GravityScale; Drag = other.Drag;
			CollideWithGround = other.CollideWithGround; GroundHeight = other.GroundHeight; Bounce = other.Bounce;
			Mode = other.Mode; Simulation = other.Simulation; TexturePath = other.TexturePath; MeshPath = other.MeshPath;
			MaterialPath = other.MaterialPath; PrefabPath = other.PrefabPath; AdditiveBlend = other.AdditiveBlend;
			ParticleMass = other.ParticleMass;
			return *this;
		}
	};

	// Serialized fields of ParticleSystemComponent (enums are handled separately)
#define LNX_PARTICLE_FIELDS(F) 	F(PlayOnStart) F(PreviewInEditor) F(Looping) F(Duration) F(Rate) F(Burst) F(MaxParticles) F(WorldSpace) 	F(ShapeRadius) F(BoxSize) F(ConeAngle) F(Lifetime) F(Speed) F(StartSize) F(EndSizeScale) F(AngularSpeed) 	F(StartColor) F(EndColor) F(GravityScale) F(Drag) F(CollideWithGround) F(GroundHeight) F(Bounce) 	F(TexturePath) F(MeshPath) F(MaterialPath) F(PrefabPath) F(AdditiveBlend) F(ParticleMass)

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using AllComponents =
		ComponentGroup<TransformComponent, SpriteRendererComponent,
		CircleRendererComponent, CameraComponent, NativeScriptComponent,
		Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent,
		Rigidbody3DComponent, BoxCollider3DComponent, SphereCollider3DComponent, 
		CapsuleCollider3DComponent, CylinderCollider3DComponent, ConeCollider3DComponent,
		MeshCollider3DComponent, CharacterController3DComponent,
		MeshComponent, MaterialComponent, LightComponent, TextureComponent, ScriptComponent,
		RelationshipComponent, EnvironmentComponent, AnimatorComponent, GroupComponent,
		ParticleSystemComponent, BlueprintComponent>;
}