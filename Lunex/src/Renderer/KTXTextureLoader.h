#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/TextureCompression.h"
#include <filesystem>

namespace Lunex {

	// ============================================================================
	// KTX TEXTURE LOADER
	// Loads KTX2 files directly (GPU-compressed textures)
	// 
	// KTX2 is the Khronos standard format for GPU-compressed textures.
	// Benefits:
	// - Direct GPU upload (no CPU decompression)
	// - Supports mipmaps
	// - Supports cubemaps and texture arrays
	// - Platform-specific supercompression (Basis Universal)
	// ============================================================================

	class KTXTextureLoader {
	public:
		// ========== LOADING ==========
		
		// Load KTX2 file to compressed data structure
		static CompressedTextureData Load(const std::filesystem::path& path);
		
		// Load KTX2 file directly to GPU texture
		static Ref<Texture2D> LoadTexture(const std::filesystem::path& path);
		
		// Load KTX2 from memory
		static CompressedTextureData LoadFromMemory(const uint8_t* data, size_t size);
		
		// ========== CONVERSION ==========
		
		// Convert standard image to KTX2 with compression
		static bool ConvertToKTX2(
			const std::filesystem::path& inputPath,
			const std::filesystem::path& outputPath,
			const TextureImportSettings& settings = TextureImportSettings::Default()
		);
		
		// Batch convert multiple files
		static void BatchConvert(
			const std::vector<std::filesystem::path>& inputPaths,
			const std::filesystem::path& outputDirectory,
			const TextureImportSettings& settings = TextureImportSettings::Default()
		);
		
		// ========== UTILITIES ==========
		
		// Check if file is a valid KTX2 file
		static bool IsKTX2File(const std::filesystem::path& path);
		
		// Get KTX2 file info without loading full data
		struct KTX2Info {
			uint32_t Width = 0;
			uint32_t Height = 0;
			uint32_t Depth = 1;
			uint32_t MipLevels = 1;
			uint32_t ArrayLayers = 1;
			bool IsCubemap = false;
			TextureCompressionFormat Format = TextureCompressionFormat::None;
			size_t DataSize = 0;
		};
		static KTX2Info GetInfo(const std::filesystem::path& path);
		
		// ========== TRANSCODING ==========
		
		// Transcode Basis Universal to platform-optimal format
		// (BC7 on desktop, ASTC on mobile, etc.)
		static CompressedTextureData Transcode(
			const CompressedTextureData& basisData,
			TextureCompressionFormat targetFormat = TextureCompressionFormat::None // None = auto-detect
		);
		
	private:
		// KTX2 file magic number
		static constexpr uint8_t KTX2_MAGIC[12] = {
			0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
		};
		
		static bool ValidateMagic(const uint8_t* data, size_t size);
	};

	// ============================================================================
	// TEXTURE IMPORT PANEL (for editor integration)
	// ============================================================================
	
	struct TextureImportResult {
		bool Success = false;
		std::string ErrorMessage;
		std::filesystem::path OutputPath;
		
		// Statistics
		size_t OriginalSize = 0;
		size_t CompressedSize = 0;
		float CompressionRatio = 0.0f;
		double CompressionTimeMs = 0.0;
	};

	// Import a texture with compression
	TextureImportResult ImportTexture(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDirectory,
		const TextureImportSettings& settings = TextureImportSettings::Default()
	);

} // namespace Lunex
