#pragma once

/**
 * @file TextureCooker.h
 * @brief Offline texture processing and cooking
 * 
 * AAA Architecture: TextureCooker lives in AssetPipeline/Texture/
 * Handles:
 * - Texture importing from various formats
 * - Compression to GPU formats (BC7, ETC2, ASTC)
 * - Mipmap generation
 * - Baking to KTX2 format
 */

#include "Core/Core.h"
#include <filesystem>
#include <vector>
#include <string>

namespace Lunex {

	// Forward declarations
	struct TextureImportSettings;
	struct CompressedTextureData;

	/**
	 * @struct TextureCookSettings
	 * @brief Settings for texture cooking process
	 */
	struct TextureCookSettings {
		// Output format
		bool CompressTexture = true;
		bool GenerateMipmaps = true;
		bool OutputKTX2 = true;
		
		// Quality
		int CompressionQuality = 128;  // 0-255
		
		// Targets
		bool TargetDesktop = true;
		bool TargetMobile = false;
		
		// Cache
		bool UseCache = true;
		std::filesystem::path CacheDirectory = ".texture_cache";
	};

	/**
	 * @struct TextureCookResult
	 * @brief Result of texture cooking
	 */
	struct TextureCookResult {
		bool Success = false;
		std::filesystem::path OutputPath;
		std::string ErrorMessage;
		
		// Stats
		size_t OriginalSize = 0;
		size_t CompressedSize = 0;
		float CompressionRatio = 1.0f;
		double CookTimeMs = 0.0;
	};

	/**
	 * @class TextureCooker
	 * @brief Cooks textures for runtime use
	 * 
	 * Usage:
	 * ```cpp
	 * TextureCookSettings settings;
	 * auto result = TextureCooker::Cook("textures/albedo.png", "cooked/albedo.ktx2", settings);
	 * ```
	 */
	class TextureCooker {
	public:
		/**
		 * @brief Cook a single texture
		 */
		static TextureCookResult Cook(
			const std::filesystem::path& inputPath,
			const std::filesystem::path& outputPath,
			const TextureCookSettings& settings = {}
		);
		
		/**
		 * @brief Cook all textures in a directory
		 */
		static std::vector<TextureCookResult> CookDirectory(
			const std::filesystem::path& inputDir,
			const std::filesystem::path& outputDir,
			const TextureCookSettings& settings = {},
			bool recursive = true
		);
		
		/**
		 * @brief Check if texture needs recooking
		 */
		static bool NeedsRecook(
			const std::filesystem::path& inputPath,
			const std::filesystem::path& outputPath
		);
		
		/**
		 * @brief Get supported input formats
		 */
		static std::vector<std::string> GetSupportedInputFormats();
		
		/**
		 * @brief Check if input format is supported
		 */
		static bool IsInputFormatSupported(const std::filesystem::path& path);
		
	private:
		TextureCooker() = delete;
	};

	/**
	 * @class TextureImporter
	 * @brief Imports textures from various formats
	 */
	class TextureImporter {
	public:
		/**
		 * @brief Import raw texture data from file
		 */
		static bool Import(
			const std::filesystem::path& path,
			std::vector<uint8_t>& outData,
			uint32_t& outWidth,
			uint32_t& outHeight,
			uint32_t& outChannels
		);
		
		/**
		 * @brief Get supported extensions
		 */
		static std::vector<std::string> GetSupportedExtensions();
	};

} // namespace Lunex
