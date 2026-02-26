#include "stpch.h"
#include "MaterialAssetFactory.h"

#include "Assets/Materials/MaterialRegistry.h"
#include "Log/Log.h"

namespace Lunex {

	std::vector<Ref<MaterialAsset>> MaterialAssetFactory::CreateFromModel(
		const Ref<Model>& model,
		const std::string& modelName)
	{
		std::vector<Ref<MaterialAsset>> materials;
		if (!model) return materials;

		const auto& meshes = model->GetMeshes();
		int materialIndex = 0;

		for (const auto& mesh : meshes) {
			const auto& matData = mesh->GetMaterialData();
			const auto& textures = mesh->GetTextures();

			// Only create a MaterialAsset if the mesh has non-default material data
			bool hasTextures = mesh->HasAnyMeshTextures();
			bool hasCustomProperties = 
				matData.BaseColor != glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) ||
				matData.Metallic > 0.001f ||
				std::abs(matData.Roughness - 0.5f) > 0.001f ||
				glm::length(matData.EmissionColor) > 0.001f;

			if (hasTextures || hasCustomProperties) {
				std::string name = modelName + "_Material_" + std::to_string(materialIndex);
				auto asset = CreateFromMeshData(matData, textures, name);
				if (asset) {
					materials.push_back(asset);
					LNX_LOG_INFO("MaterialAssetFactory: Created material '{0}' from mesh {1}",
						name, materialIndex);
				}
			}
			materialIndex++;
		}

		return materials;
	}

	Ref<MaterialAsset> MaterialAssetFactory::CreateFromMeshData(
		const MeshMaterialData& materialData,
		const std::vector<MeshTexture>& textures,
		const std::string& name)
	{
		auto asset = CreateRef<MaterialAsset>(name);

		// Set PBR properties from mesh data
		asset->SetAlbedo(materialData.BaseColor);
		asset->SetMetallic(materialData.Metallic);
		asset->SetRoughness(materialData.Roughness);
		asset->SetEmissionColor(materialData.EmissionColor);
		asset->SetEmissionIntensity(materialData.EmissionIntensity);

		// Assign textures by type
		for (const auto& tex : textures) {
			if (!tex.Texture || !tex.Texture->IsLoaded())
				continue;

			if (tex.Type == "texture_diffuse") {
				asset->SetAlbedoMap(tex.Texture);
			}
			else if (tex.Type == "texture_normal") {
				asset->SetNormalMap(tex.Texture);
			}
			else if (tex.Type == "texture_metallic") {
				asset->SetMetallicMap(tex.Texture);
			}
			else if (tex.Type == "texture_roughness") {
				asset->SetRoughnessMap(tex.Texture);
			}
			else if (tex.Type == "texture_specular") {
				asset->SetSpecularMap(tex.Texture);
			}
			else if (tex.Type == "texture_emissive") {
				asset->SetEmissionMap(tex.Texture);
			}
			else if (tex.Type == "texture_ao") {
				asset->SetAOMap(tex.Texture);
			}
		}

		// Register in MaterialRegistry for global access
		MaterialRegistry::Get().RegisterMaterial(asset);

		return asset;
	}

} // namespace Lunex
