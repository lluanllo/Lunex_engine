#include "stpch.h"
#include "MaterialImporter.h"
#include "MaterialRegistry.h"
#include "Log/Log.h"
#include "Resources/Mesh/Model.h"

#include <filesystem>

namespace Lunex {

	Ref<MaterialAsset> MaterialImporter::Import(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("MaterialImporter: File not found: {0}", path.string());
			return nullptr;
		}
		
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lumat") {
			return MaterialAsset::LoadFromFile(path);
		}
		else if (ext == ".mtl") {
			return ImportMTL(path);
		}
		else if (ext == ".gltf" || ext == ".glb") {
			LNX_LOG_WARN("MaterialImporter: GLTF import requires material name parameter");
			return nullptr;
		}
		
		LNX_LOG_ERROR("MaterialImporter: Unsupported format: {0}", ext);
		return nullptr;
	}
	
	Ref<MaterialAsset> MaterialImporter::ImportMTL(const std::filesystem::path& path) {
		LNX_LOG_WARN("MaterialImporter: MTL import not yet fully implemented");
		
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>();
		material->SetName(path.stem().string());
		
		return material;
	}
	
	Ref<MaterialAsset> MaterialImporter::ImportGLTF(const std::filesystem::path& path, const std::string& materialName) {
		LNX_LOG_WARN("MaterialImporter: GLTF import not yet fully implemented for single material");
		
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>();
		material->SetName(materialName);
		
		return material;
	}

	Ref<MaterialAsset> MaterialImporter::CreateFromModelMaterialData(
		const ModelMaterialData& matData,
		const std::filesystem::path& outputDir)
	{
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>(matData.Name);

		// Set PBR properties
		material->SetAlbedo(matData.AlbedoColor);
		material->SetMetallic(matData.Metallic);
		material->SetRoughness(matData.Roughness);
		material->SetEmissionColor(matData.EmissionColor);
		material->SetEmissionIntensity(matData.EmissionIntensity);

		// Load and assign textures
		if (!matData.AlbedoTexturePath.empty() && std::filesystem::exists(matData.AlbedoTexturePath)) {
			auto tex = Texture2D::Create(matData.AlbedoTexturePath);
			if (tex && tex->IsLoaded()) {
				material->SetAlbedoMap(tex);
				LNX_LOG_INFO("  Albedo texture: {0}", matData.AlbedoTexturePath);
			}
		}

		if (!matData.NormalTexturePath.empty() && std::filesystem::exists(matData.NormalTexturePath)) {
			auto tex = Texture2D::Create(matData.NormalTexturePath);
			if (tex && tex->IsLoaded()) {
				material->SetNormalMap(tex);
				material->SetNormalColorSpace(TextureColorSpace::NonColor);
				LNX_LOG_INFO("  Normal texture: {0}", matData.NormalTexturePath);
			}
		}

		// Handle metallic-roughness combined texture (GLTF PBR)
		if (matData.HasMetallicRoughnessMap && !matData.MetallicRoughnessTexturePath.empty()
			&& std::filesystem::exists(matData.MetallicRoughnessTexturePath)) {
			auto tex = Texture2D::Create(matData.MetallicRoughnessTexturePath);
			if (tex && tex->IsLoaded()) {
				// Use layered texture: B=metallic, G=roughness (GLTF convention)
				LayeredTextureConfig config;
				config.Texture = tex;
				config.Path = matData.MetallicRoughnessTexturePath;
				config.Enabled = true;
				config.MetallicChannel = TextureChannel::Blue;
				config.RoughnessChannel = TextureChannel::Green;
				config.AOChannel = TextureChannel::Red;
				config.UseForMetallic = true;
				config.UseForRoughness = true;
				config.UseForAO = false;
				material->SetLayeredTextureConfig(config);
				material->SetMetallicMultiplier(matData.Metallic > 0.0f ? matData.Metallic : 1.0f);
				material->SetRoughnessMultiplier(matData.Roughness > 0.0f ? matData.Roughness : 1.0f);
				LNX_LOG_INFO("  MetallicRoughness layered texture: {0}", matData.MetallicRoughnessTexturePath);
			}
		}
		else {
			// Separate metallic/roughness textures
			if (!matData.MetallicTexturePath.empty() && std::filesystem::exists(matData.MetallicTexturePath)) {
				auto tex = Texture2D::Create(matData.MetallicTexturePath);
				if (tex && tex->IsLoaded()) {
					material->SetMetallicMap(tex);
					material->SetMetallicColorSpace(TextureColorSpace::NonColor);
					material->SetMetallicMultiplier(matData.Metallic > 0.0f ? matData.Metallic : 1.0f);
					LNX_LOG_INFO("  Metallic texture: {0}", matData.MetallicTexturePath);
				}
			}

			if (!matData.RoughnessTexturePath.empty() && std::filesystem::exists(matData.RoughnessTexturePath)) {
				auto tex = Texture2D::Create(matData.RoughnessTexturePath);
				if (tex && tex->IsLoaded()) {
					material->SetRoughnessMap(tex);
					material->SetRoughnessColorSpace(TextureColorSpace::NonColor);
					material->SetRoughnessMultiplier(matData.Roughness > 0.0f ? matData.Roughness : 1.0f);
					LNX_LOG_INFO("  Roughness texture: {0}", matData.RoughnessTexturePath);
				}
			}
		}

		if (!matData.AOTexturePath.empty() && std::filesystem::exists(matData.AOTexturePath)) {
			auto tex = Texture2D::Create(matData.AOTexturePath);
			if (tex && tex->IsLoaded()) {
				material->SetAOMap(tex);
				material->SetAOColorSpace(TextureColorSpace::NonColor);
				LNX_LOG_INFO("  AO texture: {0}", matData.AOTexturePath);
			}
		}

		if (!matData.EmissionTexturePath.empty() && std::filesystem::exists(matData.EmissionTexturePath)) {
			auto tex = Texture2D::Create(matData.EmissionTexturePath);
			if (tex && tex->IsLoaded()) {
				material->SetEmissionMap(tex);
				LNX_LOG_INFO("  Emission texture: {0}", matData.EmissionTexturePath);
			}
		}

		// Save to disk as .lumat
		if (!outputDir.empty()) {
			std::filesystem::create_directories(outputDir);

			// Sanitize material name for filename
			std::string safeName = matData.Name;
			for (char& c : safeName) {
				if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ' ') {
					c = '_';
				}
			}

			std::filesystem::path lumatPath = outputDir / (safeName + ".lumat");
			material->SaveToFile(lumatPath);
			material->SetPath(lumatPath);

			// Register in the MaterialRegistry
			MaterialRegistry::Get().RegisterMaterial(material);

			LNX_LOG_INFO("Created .lumat material: {0}", lumatPath.string());
		}

		return material;
	}

	std::vector<Ref<MaterialAsset>> MaterialImporter::CreateMaterialsFromModel(
		const Ref<Model>& model,
		const std::filesystem::path& outputDir)
	{
		std::vector<Ref<MaterialAsset>> materials;

		if (!model || !model->HasMaterialData()) {
			LNX_LOG_WARN("MaterialImporter::CreateMaterialsFromModel - No material data in model");
			return materials;
		}

		const auto& materialDataList = model->GetMaterialData();
		
		// Track already-created materials by name to avoid duplicates
		std::unordered_map<std::string, Ref<MaterialAsset>> createdMaterials;

		for (size_t i = 0; i < materialDataList.size(); i++) {
			const auto& matData = materialDataList[i];

			// Check for duplicate material names
			auto it = createdMaterials.find(matData.Name);
			if (it != createdMaterials.end()) {
				materials.push_back(it->second);
				continue;
			}

			// Check if .lumat already exists on disk
			std::string safeName = matData.Name;
			for (char& c : safeName) {
				if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ' ') {
					c = '_';
				}
			}

			std::filesystem::path existingLumat = outputDir / (safeName + ".lumat");
			if (std::filesystem::exists(existingLumat)) {
				auto existing = MaterialAsset::LoadFromFile(existingLumat);
				if (existing) {
					MaterialRegistry::Get().RegisterMaterial(existing);
					materials.push_back(existing);
					createdMaterials[matData.Name] = existing;
					LNX_LOG_INFO("Loaded existing material: {0}", existingLumat.string());
					continue;
				}
			}

			Ref<MaterialAsset> mat = CreateFromModelMaterialData(matData, outputDir);
			if (mat) {
				materials.push_back(mat);
				createdMaterials[matData.Name] = mat;
			}
		}

		LNX_LOG_INFO("MaterialImporter: Created {0} materials from model", materials.size());
		return materials;
	}
	
	std::vector<std::string> MaterialImporter::GetSupportedExtensions() {
		return { ".lumat", ".mtl", ".gltf", ".glb" };
	}
	
	bool MaterialImporter::IsSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		auto supported = GetSupportedExtensions();
		return std::find(supported.begin(), supported.end(), ext) != supported.end();
	}

} // namespace Lunex
