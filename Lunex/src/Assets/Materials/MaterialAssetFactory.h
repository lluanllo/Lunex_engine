#pragma once

/**
 * @file MaterialAssetFactory.h
 * @brief Factory for creating MaterialAssets from GLTF/Assimp imported mesh data
 * 
 * When a GLTF model is imported, each sub-mesh may have its own PBR material
 * properties and textures stored in MeshMaterialData and MeshTexture.
 * This factory converts them into proper MaterialAsset objects that integrate
 * with the material editor and serialization system.
 */

#include "Core/Core.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Resources/Mesh/Mesh.h"
#include "Resources/Mesh/Model.h"

#include <vector>
#include <string>
#include <filesystem>

namespace Lunex {

	class MaterialAssetFactory {
	public:
		/**
		 * @brief Create MaterialAssets from all sub-meshes of a Model
		 * @param model The loaded model with per-mesh material data
		 * @param modelName Base name for generated materials
		 * @return Vector of MaterialAssets, one per unique material in the model
		 */
		static std::vector<Ref<MaterialAsset>> CreateFromModel(
			const Ref<Model>& model,
			const std::string& modelName = "Imported");

		/**
		 * @brief Create a single MaterialAsset from mesh material data and textures
		 * @param materialData PBR properties from the mesh
		 * @param textures Texture references from the mesh
		 * @param name Name for the material
		 * @return New MaterialAsset configured with the mesh data
		 */
		static Ref<MaterialAsset> CreateFromMeshData(
			const MeshMaterialData& materialData,
			const std::vector<MeshTexture>& textures,
			const std::string& name = "Imported Material");

	private:
		MaterialAssetFactory() = delete;
	};

} // namespace Lunex
