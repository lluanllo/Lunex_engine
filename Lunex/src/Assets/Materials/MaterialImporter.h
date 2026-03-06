#pragma once

/**
 * @file MaterialImporter.h
 * @brief Material importing from various formats
 * 
 * AAA Architecture: MaterialImporter handles loading external material formats.
 */

#include "MaterialAsset.h"
#include "Core/Core.h"
#include <filesystem>

namespace Lunex {

	// Forward declarations
	class Model;
	struct ModelMaterialData;

	/**
	 * @class MaterialImporter
	 * @brief Imports materials from various file formats
	 */
	class MaterialImporter {
	public:
		/**
		 * @brief Import material from file
		 * @param path Path to material file (.lumat, .mtl, etc.)
		 * @return Loaded MaterialAsset or nullptr on failure
		 */
		static Ref<MaterialAsset> Import(const std::filesystem::path& path);
		
		/**
		 * @brief Import material from OBJ .mtl file
		 */
		static Ref<MaterialAsset> ImportMTL(const std::filesystem::path& path);
		
		/**
		 * @brief Import material from glTF/GLB
		 */
		static Ref<MaterialAsset> ImportGLTF(const std::filesystem::path& path, const std::string& materialName);

		/**
		 * @brief Create a MaterialAsset from extracted model material data
		 * @param matData Extracted PBR material data from model import
		 * @param outputDir Directory to save the .lumat file
		 * @return Created MaterialAsset
		 */
		static Ref<MaterialAsset> CreateFromModelMaterialData(
			const ModelMaterialData& matData,
			const std::filesystem::path& outputDir = "");

		/**
		 * @brief Create all materials from a loaded Model, saving as .lumat files
		 * @param model The loaded model with material data
		 * @param outputDir Directory to save .lumat files (e.g., Assets/Materials/)
		 * @return Vector of created MaterialAssets (one per sub-mesh)
		 */
		static std::vector<Ref<MaterialAsset>> CreateMaterialsFromModel(
			const Ref<Model>& model,
			const std::filesystem::path& outputDir);
		
		/**
		 * @brief Get list of supported file extensions
		 */
		static std::vector<std::string> GetSupportedExtensions();
		
		/**
		 * @brief Check if file extension is supported
		 */
		static bool IsSupported(const std::filesystem::path& path);
	};

} // namespace Lunex
