#include "stpch.h"
#include "KTXTextureLoader.h"
#include "Log/Log.h"

#include <fstream>
#include <chrono>

// ============================================================================
// KTX-Software Integration Notes
// ============================================================================
// When you install KTX-Software, uncomment:
// #include <ktx.h>
//
// And replace the placeholder implementations below with actual KTX calls.
// ============================================================================

namespace Lunex {

	// ========== LOADING ==========

	CompressedTextureData KTXTextureLoader::Load(const std::filesystem::path& path) {
		CompressedTextureData result;
		
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("KTX file not found: {0}", path.string());
			return result;
		}
		
		// Check if it's our custom format (.lnxtex) or actual KTX2
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lnxtex") {
			// Load our custom cached format
			return TextureCompressor::Get().LoadFromKTX2(path);
		}
		
		if (ext == ".ktx2" || ext == ".ktx") {
			// TODO: Load actual KTX2 with libktx
			// For now, return placeholder implementation
			
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file) {
				LNX_LOG_ERROR("Failed to open KTX file: {0}", path.string());
				return result;
			}
			
			size_t fileSize = file.tellg();
			file.seekg(0);
			
			std::vector<uint8_t> fileData(fileSize);
			file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
			
			if (fileSize >= 12 && ValidateMagic(fileData.data(), fileSize)) {
				// It's a valid KTX2 file
				LNX_LOG_WARN("KTX2 loading requires KTX-Software library. Install from: https://github.com/KhronosGroup/KTX-Software");
				
				// TODO: Implement with ktxTexture2_CreateFromMemory()
				// ktxTexture2* ktxTex = nullptr;
				// KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
				//     fileData.data(), fileData.size(),
				//     KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
				//     &ktxTex
				// );
				// 
				// if (ktxResult == KTX_SUCCESS) {
				//     result.Width = ktxTex->baseWidth;
				//     result.Height = ktxTex->baseHeight;
				//     result.MipLevels = ...
				//     // etc.
				// }
			}
		}
		
		return result;
	}

	Ref<Texture2D> KTXTextureLoader::LoadTexture(const std::filesystem::path& path) {
		CompressedTextureData data = Load(path);
		
		if (!data.IsValid()) {
			LNX_LOG_ERROR("Failed to load KTX texture: {0}", path.string());
			return nullptr;
		}
		
		return Texture2D::CreateCompressed(data);
	}

	CompressedTextureData KTXTextureLoader::LoadFromMemory(const uint8_t* data, size_t size) {
		CompressedTextureData result;
		
		if (!data || size < 12) {
			LNX_LOG_ERROR("Invalid KTX data");
			return result;
		}
		
		if (!ValidateMagic(data, size)) {
			LNX_LOG_ERROR("Invalid KTX2 magic number");
			return result;
		}
		
		// TODO: Implement with ktxTexture2_CreateFromMemory()
		LNX_LOG_WARN("KTX2 loading requires KTX-Software library");
		
		return result;
	}

	// ========== CONVERSION ==========

	bool KTXTextureLoader::ConvertToKTX2(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& outputPath,
		const TextureImportSettings& settings
	) {
		auto& compressor = TextureCompressor::Get();
		if (!compressor.IsInitialized()) {
			compressor.Initialize();
		}
		
		// Compress the texture
		CompressedTextureData compressed = compressor.CompressFromFile(inputPath, settings);
		
		if (!compressed.IsValid()) {
			LNX_LOG_ERROR("Failed to compress texture: {0}", inputPath.string());
			return false;
		}
		
		// Save to output path
		return compressor.SaveToKTX2(compressed, outputPath);
	}

	void KTXTextureLoader::BatchConvert(
		const std::vector<std::filesystem::path>& inputPaths,
		const std::filesystem::path& outputDirectory,
		const TextureImportSettings& settings
	) {
		if (!std::filesystem::exists(outputDirectory)) {
			std::filesystem::create_directories(outputDirectory);
		}
		
		size_t success = 0;
		size_t failed = 0;
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		for (const auto& input : inputPaths) {
			std::filesystem::path output = outputDirectory / (input.stem().string() + ".lnxtex");
			
			if (ConvertToKTX2(input, output, settings)) {
				success++;
				LNX_LOG_INFO("Converted: {0}", input.filename().string());
			}
			else {
				failed++;
				LNX_LOG_ERROR("Failed: {0}", input.filename().string());
			}
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		
		LNX_LOG_INFO("Batch conversion complete: {0} success, {1} failed, {2}ms", 
			success, failed, duration);
	}

	// ========== UTILITIES ==========

	bool KTXTextureLoader::IsKTX2File(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			return false;
		}
		
		std::ifstream file(path, std::ios::binary);
		if (!file) {
			return false;
		}
		
		uint8_t magic[12];
		file.read(reinterpret_cast<char*>(magic), 12);
		
		return file.good() && ValidateMagic(magic, 12);
	}

	KTXTextureLoader::KTX2Info KTXTextureLoader::GetInfo(const std::filesystem::path& path) {
		KTX2Info info;
		
		if (!std::filesystem::exists(path)) {
			return info;
		}
		
		// For our custom format
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lnxtex") {
			std::ifstream file(path, std::ios::binary);
			if (!file) return info;
			
			// Read our header
			struct LnxTexHeader {
				char Magic[4];
				uint32_t Version;
				uint32_t Width;
				uint32_t Height;
				uint32_t Format;
				uint32_t MipCount;
				uint32_t DataSize;
				uint8_t IsSRGB;
				uint8_t Reserved[3];
			};
			
			LnxTexHeader header;
			file.read(reinterpret_cast<char*>(&header), sizeof(header));
			
			if (header.Magic[0] == 'L' && header.Magic[1] == 'N' &&
				header.Magic[2] == 'X' && header.Magic[3] == 'T') {
				info.Width = header.Width;
				info.Height = header.Height;
				info.MipLevels = header.MipCount;
				info.Format = static_cast<TextureCompressionFormat>(header.Format);
				info.DataSize = header.DataSize;
			}
		}
		else if (ext == ".ktx2" || ext == ".ktx") {
			// TODO: Parse KTX2 header with libktx
			LNX_LOG_WARN("KTX2 info parsing requires KTX-Software library");
		}
		
		return info;
	}

	// ========== TRANSCODING ==========

	CompressedTextureData KTXTextureLoader::Transcode(
		const CompressedTextureData& basisData,
		TextureCompressionFormat targetFormat
	) {
		CompressedTextureData result;
		
		if (basisData.Format != TextureCompressionFormat::BasisUniversal) {
			LNX_LOG_WARN("Transcode only works with Basis Universal textures");
			return basisData; // Return as-is
		}
		
		// Auto-detect best format for current platform
		if (targetFormat == TextureCompressionFormat::None) {
			// Check what's supported
			if (TextureCompressor::IsFormatSupported(TextureCompressionFormat::BC7)) {
				targetFormat = TextureCompressionFormat::BC7;
			}
			else if (TextureCompressor::IsFormatSupported(TextureCompressionFormat::ASTC_4x4)) {
				targetFormat = TextureCompressionFormat::ASTC_4x4;
			}
			else if (TextureCompressor::IsFormatSupported(TextureCompressionFormat::ETC2_RGBA)) {
				targetFormat = TextureCompressionFormat::ETC2_RGBA;
			}
			else {
				targetFormat = TextureCompressionFormat::None; // Decompress to RGBA
			}
		}
		
		// TODO: Implement with ktxTexture2_TranscodeBasis()
		// ktxTexture2* ktxTex = ...;
		// ktxTexture2_TranscodeBasis(ktxTex, targetFormat, 0);
		
		LNX_LOG_WARN("Basis Universal transcoding requires KTX-Software library");
		
		return result;
	}

	bool KTXTextureLoader::ValidateMagic(const uint8_t* data, size_t size) {
		if (size < 12) return false;
		
		for (int i = 0; i < 12; i++) {
			if (data[i] != KTX2_MAGIC[i]) {
				return false;
			}
		}
		return true;
	}

	// ========== TEXTURE IMPORT ==========

	TextureImportResult ImportTexture(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDirectory,
		const TextureImportSettings& settings
	) {
		TextureImportResult result;
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// Check source exists
		if (!std::filesystem::exists(sourcePath)) {
			result.Success = false;
			result.ErrorMessage = "Source file not found: " + sourcePath.string();
			return result;
		}
		
		// Get original size
		result.OriginalSize = std::filesystem::file_size(sourcePath);
		
		// Create output directory
		if (!std::filesystem::exists(outputDirectory)) {
			std::filesystem::create_directories(outputDirectory);
		}
		
		// Generate output path
		result.OutputPath = outputDirectory / (sourcePath.stem().string() + ".lnxtex");
		
		// Compress
		auto& compressor = TextureCompressor::Get();
		if (!compressor.IsInitialized()) {
			compressor.Initialize();
		}
		
		CompressedTextureData compressed = compressor.CompressFromFile(sourcePath, settings);
		
		if (!compressed.IsValid()) {
			result.Success = false;
			result.ErrorMessage = "Compression failed";
			return result;
		}
		
		// Save
		if (!compressor.SaveToKTX2(compressed, result.OutputPath)) {
			result.Success = false;
			result.ErrorMessage = "Failed to save compressed texture";
			return result;
		}
		
		// Calculate stats
		result.CompressedSize = std::filesystem::file_size(result.OutputPath);
		result.CompressionRatio = static_cast<float>(result.OriginalSize) / 
								   static_cast<float>(result.CompressedSize);
		
		auto endTime = std::chrono::high_resolution_clock::now();
		result.CompressionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
		
		result.Success = true;
		
		LNX_LOG_INFO("Imported texture: {0} -> {1} (ratio: {2:.2f}x, time: {3:.1f}ms)",
			sourcePath.filename().string(),
			result.OutputPath.filename().string(),
			result.CompressionRatio,
			result.CompressionTimeMs);
		
		return result;
	}

} // namespace Lunex
