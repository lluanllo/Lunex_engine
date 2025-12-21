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
		 * @brief Get list of supported file extensions
		 */
		static std::vector<std::string> GetSupportedExtensions();
		
		/**
		 * @brief Check if file extension is supported
		 */
		static bool IsSupported(const std::filesystem::path& path);
	};

} // namespace Lunex
