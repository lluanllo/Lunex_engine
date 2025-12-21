#include "stpch.h"
#include "TextureCompression.h"
#include "Log/Log.h"

// ============================================================================
// KTX-Software Integration
// ============================================================================
#ifdef LNX_HAS_KTX
#include <ktx.h>
#define KTX_ENABLED 1
#else
#define KTX_ENABLED 0
#endif

#include <glad/glad.h>
#include <fstream>
#include <chrono>

// stb_image for loading source textures
#include "stb_image.h"

// ============================================================================
// GL EXTENSION CONSTANTS (may not be defined in all GLAD versions)
// ============================================================================

// S3TC/DXT compression
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT           0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT          0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT          0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT          0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT          0x8C4C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT    0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT    0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT    0x8C4F
#endif

// RGTC compression
#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1                   0x8DBB
#endif
#ifndef GL_COMPRESSED_SIGNED_RED_RGTC1
#define GL_COMPRESSED_SIGNED_RED_RGTC1            0x8DBC
#endif
#ifndef GL_COMPRESSED_RG_RGTC2
#define GL_COMPRESSED_RG_RGTC2                    0x8DBD
#endif
#ifndef GL_COMPRESSED_SIGNED_RG_RGTC2
#define GL_COMPRESSED_SIGNED_RG_RGTC2             0x8DBE
#endif

// BPTC compression (BC7)
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM             0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM       0x8E8D
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT       0x8E8E
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT     0x8E8F
#endif

// ETC2 compression
#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2                   0x9274
#endif
#ifndef GL_COMPRESSED_SRGB8_ETC2
#define GL_COMPRESSED_SRGB8_ETC2                  0x9275
#endif
#ifndef GL_COMPRESSED_RGBA8_ETC2_EAC
#define GL_COMPRESSED_RGBA8_ETC2_EAC              0x9278
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC       0x9279
#endif

// ASTC compression
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR           0x93B0
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_6x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR           0x93B4
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_8x8_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR           0x93B7
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR   0x93D0
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR   0x93D4
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR   0x93D7
#endif

namespace Lunex {

	// ============================================================================
	// TEXTURE COMPRESSION CONFIG
	// ============================================================================
	
	TextureImportSettings TextureCompressionConfig::GetDefaultSettings() const {
		TextureImportSettings settings;
		
		// If KTX is not available or auto-compression is disabled, use None
		if (!EnableAutoCompression || !IsKTXAvailable()) {
			settings.CompressionFormat = TextureCompressionFormat::None;
		}
		else {
			settings.CompressionFormat = DefaultFormat;
		}
		
		settings.CompressionQuality = DefaultQuality;
		settings.GenerateMipmaps = GenerateMipmaps;
		settings.UseCache = UseCache;
		settings.IsSRGB = true;
		
		return settings;
	}

	// ============================================================================
	// TEXTURE IMPORT SETTINGS
	// ============================================================================

	TextureCompressionFormat TextureImportSettings::GetRecommendedFormat() const {
		if (IsNormalMap) {
			return TextureCompressionFormat::BC5; // Best for normal maps
		}
		
		if (IsHDR) {
			return TextureCompressionFormat::BC7; // HDR needs high quality
		}
		
		if (!PreserveAlpha) {
			return TextureCompressionFormat::BC1; // RGB only, smallest
		}
		
		// Default: high quality RGBA
		return TextureCompressionFormat::BC7;
	}

	std::string TextureImportSettings::Serialize() const {
		std::stringstream ss;
		ss << "format=" << static_cast<int>(CompressionFormat) << ";";
		ss << "quality=" << CompressionQuality << ";";
		ss << "mipmaps=" << (GenerateMipmaps ? 1 : 0) << ";";
		ss << "maxMips=" << MaxMipLevels << ";";
		ss << "maxW=" << MaxWidth << ";";
		ss << "maxH=" << MaxHeight << ";";
		ss << "pot=" << (PowerOfTwo ? 1 : 0) << ";";
		ss << "srgb=" << (IsSRGB ? 1 : 0) << ";";
		ss << "normal=" << (IsNormalMap ? 1 : 0) << ";";
		ss << "hdr=" << (IsHDR ? 1 : 0) << ";";
		ss << "alpha=" << (PreserveAlpha ? 1 : 0) << ";";
		ss << "premult=" << (PremultiplyAlpha ? 1 : 0) << ";";
		return ss.str();
	}

	TextureImportSettings TextureImportSettings::Deserialize(const std::string& data) {
		TextureImportSettings settings;
		
		// Parse key=value pairs
		std::istringstream ss(data);
		std::string token;
		
		while (std::getline(ss, token, ';')) {
			size_t eq = token.find('=');
			if (eq == std::string::npos) continue;
			
			std::string key = token.substr(0, eq);
			std::string value = token.substr(eq + 1);
			
			try {
				if (key == "format") settings.CompressionFormat = static_cast<TextureCompressionFormat>(std::stoi(value));
				else if (key == "quality") settings.CompressionQuality = std::stoi(value);
				else if (key == "mipmaps") settings.GenerateMipmaps = std::stoi(value) != 0;
				else if (key == "maxMips") settings.MaxMipLevels = std::stoi(value);
				else if (key == "maxW") settings.MaxWidth = std::stoul(value);
				else if (key == "maxH") settings.MaxHeight = std::stoul(value);
				else if (key == "pot") settings.PowerOfTwo = std::stoi(value) != 0;
				else if (key == "srgb") settings.IsSRGB = std::stoi(value) != 0;
				else if (key == "normal") settings.IsNormalMap = std::stoi(value) != 0;
				else if (key == "hdr") settings.IsHDR = std::stoi(value) != 0;
				else if (key == "alpha") settings.PreserveAlpha = std::stoi(value) != 0;
				else if (key == "premult") settings.PremultiplyAlpha = std::stoi(value) != 0;
			}
			catch (...) {
				// Ignore parse errors
			}
		}
		
		return settings;
	}

	TextureImportSettings TextureImportSettings::Default() {
		TextureImportSettings settings;
		settings.CompressionFormat = TextureCompressionFormat::BC7;
		settings.CompressionQuality = 128;
		settings.GenerateMipmaps = true;
		settings.IsSRGB = true;
		return settings;
	}

	TextureImportSettings TextureImportSettings::NormalMap() {
		TextureImportSettings settings;
		settings.CompressionFormat = TextureCompressionFormat::BC5;
		settings.CompressionQuality = 200; // High quality for normals
		settings.GenerateMipmaps = true;
		settings.IsSRGB = false; // Normal maps are linear
		settings.IsNormalMap = true;
		settings.PreserveAlpha = false;
		return settings;
	}

	TextureImportSettings TextureImportSettings::HDR() {
		TextureImportSettings settings;
		settings.CompressionFormat = TextureCompressionFormat::BC7;
		settings.CompressionQuality = 255; // Maximum quality
		settings.GenerateMipmaps = true;
		settings.IsSRGB = false;
		settings.IsHDR = true;
		return settings;
	}

	TextureImportSettings TextureImportSettings::UI() {
		TextureImportSettings settings;
		settings.CompressionFormat = TextureCompressionFormat::BC7;
		settings.CompressionQuality = 200; // High quality for UI
		settings.GenerateMipmaps = false; // UI doesn't need mipmaps
		settings.IsSRGB = true;
		return settings;
	}

	TextureImportSettings TextureImportSettings::Sprite() {
		TextureImportSettings settings;
		settings.CompressionFormat = TextureCompressionFormat::BC3; // Good alpha support
		settings.CompressionQuality = 180;
		settings.GenerateMipmaps = true;
		settings.IsSRGB = true;
		return settings;
	}

	// ============================================================================
	// COMPRESSED TEXTURE DATA
	// ============================================================================

	const uint8_t* CompressedTextureData::GetMipData(uint32_t level) const {
		if (level >= MipLevels.size()) return nullptr;
		return Data.data() + MipLevels[level].DataOffset;
	}

	size_t CompressedTextureData::GetMipDataSize(uint32_t level) const {
		if (level >= MipLevels.size()) return 0;
		return MipLevels[level].DataSize;
	}

	// ============================================================================
	// TEXTURE COMPRESSOR - Singleton
	// ============================================================================

	TextureCompressor& TextureCompressor::Get() {
		static TextureCompressor instance;
		return instance;
	}

	void TextureCompressor::Initialize() {
		if (m_Initialized) return;
		
		LNX_LOG_INFO("Initializing Texture Compression System");
		
		// Set default cache directory
		m_CacheDirectory = std::filesystem::current_path() / ".texture_cache";
		
		// Create cache directory if it doesn't exist
		if (!std::filesystem::exists(m_CacheDirectory)) {
			std::filesystem::create_directories(m_CacheDirectory);
		}
		
		// TODO: Initialize KTX library when available
		// ktxTexture_CreateFromNamedFile etc.
		
		m_Initialized = true;
		LNX_LOG_INFO("Texture Compression System initialized. Cache: {0}", m_CacheDirectory.string());
	}

	void TextureCompressor::Shutdown() {
		if (!m_Initialized) return;
		
		LNX_LOG_INFO("Shutting down Texture Compression System");
		m_Initialized = false;
	}

	// ============================================================================
	// COMPRESSION
	// ============================================================================

	CompressedTextureData TextureCompressor::Compress(
		const uint8_t* data,
		uint32_t width,
		uint32_t height,
		uint32_t channels,
		const TextureImportSettings& settings
	) {
		CompressedTextureData result;
		
		if (!data || width == 0 || height == 0) {
			LNX_LOG_ERROR("TextureCompressor::Compress - Invalid input data");
			return result;
		}
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// For now, without KTX library, store uncompressed
		// TODO: Implement actual compression with KTX-Software
		
		result.Width = width;
		result.Height = height;
		result.Format = settings.CompressionFormat;
		result.IsSRGB = settings.IsSRGB;
		result.InternalFormat = GetGLInternalFormat(settings.CompressionFormat, settings.IsSRGB);
		
		// Without KTX: just store raw data
		size_t dataSize = width * height * channels;
		result.Data.resize(dataSize);
		memcpy(result.Data.data(), data, dataSize);
		
		// Single mip level
		CompressedMipLevel mip;
		mip.Width = width;
		mip.Height = height;
		mip.DataSize = static_cast<uint32_t>(dataSize);
		mip.DataOffset = 0;
		result.MipLevels.push_back(mip);
		
		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		
		LNX_LOG_INFO("Compressed texture {0}x{1} in {2}ms (placeholder - KTX not installed)", 
			width, height, duration);
		
		return result;
	}

	CompressedTextureData TextureCompressor::CompressFromFile(
		const std::filesystem::path& sourcePath,
		const TextureImportSettings& settings
	) {
		CompressedTextureData result;
		
		// Check cache first
		if (settings.UseCache && !settings.ForceRecompress) {
			auto cachePath = GetCachePath(sourcePath);
			if (IsCacheValid(sourcePath)) {
				result = LoadFromKTX2(cachePath);
				if (result.IsValid()) {
					LNX_LOG_TRACE("Loaded compressed texture from cache: {0}", 
						sourcePath.filename().string());
					return result;
				}
			}
		}
		
		// Load source image
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		uint8_t* data = stbi_load(sourcePath.string().c_str(), &width, &height, &channels, 0);
		
		if (!data) {
			LNX_LOG_ERROR("Failed to load texture for compression: {0}", sourcePath.string());
			return result;
		}
		
		// Compress
		result = Compress(data, width, height, channels, settings);
		
		stbi_image_free(data);
		
		// Save to cache
		if (settings.UseCache && result.IsValid()) {
			auto cachePath = GetCachePath(sourcePath);
			SaveToKTX2(result, cachePath);
		}
		
		return result;
	}

	// ============================================================================
	// KTX2 FILE OPERATIONS
	// ============================================================================

	bool TextureCompressor::SaveToKTX2(
		const CompressedTextureData& data,
		const std::filesystem::path& outputPath
	) {
		if (!data.IsValid()) {
			LNX_LOG_ERROR("Cannot save invalid texture data to KTX2");
			return false;
		}
		
		// Ensure directory exists
		std::filesystem::create_directories(outputPath.parent_path());
		
		// TODO: Use KTX library for proper KTX2 format
		// For now, save as raw binary with header
		
		std::ofstream file(outputPath, std::ios::binary);
		if (!file) {
			LNX_LOG_ERROR("Failed to create KTX2 file: {0}", outputPath.string());
			return false;
		}
		
		// Simple header
		struct LnxTexHeader {
			char Magic[4] = {'L', 'N', 'X', 'T'};
			uint32_t Version = 1;
			uint32_t Width;
			uint32_t Height;
			uint32_t Format;
			uint32_t MipCount;
			uint32_t DataSize;
			uint8_t IsSRGB;
			uint8_t Reserved[3] = {0};
		};
		
		LnxTexHeader header;
		header.Width = data.Width;
		header.Height = data.Height;
		header.Format = static_cast<uint32_t>(data.Format);
		header.MipCount = data.GetMipCount();
		header.DataSize = static_cast<uint32_t>(data.Data.size());
		header.IsSRGB = data.IsSRGB ? 1 : 0;
		
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		
		// Write mip level info
		for (const auto& mip : data.MipLevels) {
			file.write(reinterpret_cast<const char*>(&mip), sizeof(CompressedMipLevel));
		}
		
		// Write data
		file.write(reinterpret_cast<const char*>(data.Data.data()), data.Data.size());
		
		file.close();
		
		LNX_LOG_TRACE("Saved texture cache: {0}", outputPath.filename().string());
		return true;
	}

	CompressedTextureData TextureCompressor::LoadFromKTX2(
		const std::filesystem::path& ktxPath
	) {
		CompressedTextureData result;
		
		if (!std::filesystem::exists(ktxPath)) {
			return result;
		}
		
		std::ifstream file(ktxPath, std::ios::binary);
		if (!file) {
			LNX_LOG_ERROR("Failed to open KTX2 file: {0}", ktxPath.string());
			return result;
		}
		
		// Read header
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
		
		// Validate magic
		if (header.Magic[0] != 'L' || header.Magic[1] != 'N' || 
			header.Magic[2] != 'X' || header.Magic[3] != 'T') {
			LNX_LOG_ERROR("Invalid texture cache file: {0}", ktxPath.string());
			return result;
		}
		
		result.Width = header.Width;
		result.Height = header.Height;
		result.Format = static_cast<TextureCompressionFormat>(header.Format);
		result.IsSRGB = header.IsSRGB != 0;
		result.InternalFormat = GetGLInternalFormat(result.Format, result.IsSRGB);
		
		// Read mip levels
		result.MipLevels.resize(header.MipCount);
		for (uint32_t i = 0; i < header.MipCount; i++) {
			file.read(reinterpret_cast<char*>(&result.MipLevels[i]), sizeof(CompressedMipLevel));
		}
		
		// Read data
		result.Data.resize(header.DataSize);
		file.read(reinterpret_cast<char*>(result.Data.data()), header.DataSize);
		
		return result;
	}

	// ============================================================================
	// CACHE OPERATIONS
	// ============================================================================

	std::filesystem::path TextureCompressor::GetCachePath(const std::filesystem::path& sourcePath) const {
		// Create a unique cache filename based on source path
		std::string filename = sourcePath.stem().string();
		
		// Add hash of full path to avoid collisions
		size_t hash = std::hash<std::string>{}(sourcePath.string());
		filename += "_" + std::to_string(hash) + ".lnxtex";
		
		return m_CacheDirectory / filename;
	}

	bool TextureCompressor::IsCacheValid(const std::filesystem::path& sourcePath) const {
		auto cachePath = GetCachePath(sourcePath);
		
		if (!std::filesystem::exists(cachePath) || !std::filesystem::exists(sourcePath)) {
			return false;
		}
		
		// Check if source is newer than cache
		auto sourceTime = std::filesystem::last_write_time(sourcePath);
		auto cacheTime = std::filesystem::last_write_time(cachePath);
		
		return cacheTime >= sourceTime;
	}

	void TextureCompressor::ClearCache() {
		if (std::filesystem::exists(m_CacheDirectory)) {
			std::filesystem::remove_all(m_CacheDirectory);
			std::filesystem::create_directories(m_CacheDirectory);
			LNX_LOG_INFO("Cleared texture cache");
		}
	}

	void TextureCompressor::ClearCacheForFile(const std::filesystem::path& sourcePath) {
		auto cachePath = GetCachePath(sourcePath);
		if (std::filesystem::exists(cachePath)) {
			std::filesystem::remove(cachePath);
			LNX_LOG_INFO("Cleared cache for: {0}", sourcePath.filename().string());
		}
	}

	void TextureCompressor::SetCacheDirectory(const std::filesystem::path& path) {
		m_CacheDirectory = path;
		if (!std::filesystem::exists(m_CacheDirectory)) {
			std::filesystem::create_directories(m_CacheDirectory);
		}
		LNX_LOG_INFO("Texture cache directory set to: {0}", m_CacheDirectory.string());
	}

	// ============================================================================
	// UTILITIES
	// ============================================================================

	uint32_t TextureCompressor::GetGLInternalFormat(TextureCompressionFormat format, bool sRGB) {
		switch (format) {
			case TextureCompressionFormat::None:
				return sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
				
			case TextureCompressionFormat::BC1:
				return sRGB ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				
			case TextureCompressionFormat::BC3:
				return sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				
			case TextureCompressionFormat::BC4:
				return GL_COMPRESSED_RED_RGTC1; // No sRGB variant
				
			case TextureCompressionFormat::BC5:
				return GL_COMPRESSED_RG_RGTC2; // No sRGB variant
				
			case TextureCompressionFormat::BC7:
				return sRGB ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;
				
			// ETC formats (OpenGL ES)
			case TextureCompressionFormat::ETC2_RGB:
				return sRGB ? GL_COMPRESSED_SRGB8_ETC2 : GL_COMPRESSED_RGB8_ETC2;
				
			case TextureCompressionFormat::ETC2_RGBA:
				return sRGB ? GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC : GL_COMPRESSED_RGBA8_ETC2_EAC;
				
			// ASTC formats
			case TextureCompressionFormat::ASTC_4x4:
				return sRGB ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
				
			case TextureCompressionFormat::ASTC_6x6:
				return sRGB ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR : GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
				
			case TextureCompressionFormat::ASTC_8x8:
				return sRGB ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR : GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
				
			default:
				return sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
		}
	}

	uint32_t TextureCompressor::GetBytesPerBlock(TextureCompressionFormat format) {
		switch (format) {
			case TextureCompressionFormat::BC1:
			case TextureCompressionFormat::BC4:
			case TextureCompressionFormat::ETC1:
			case TextureCompressionFormat::ETC2_RGB:
				return 8;
				
			case TextureCompressionFormat::BC3:
			case TextureCompressionFormat::BC5:
			case TextureCompressionFormat::BC7:
			case TextureCompressionFormat::ETC2_RGBA:
			case TextureCompressionFormat::ASTC_4x4:
				return 16;
				
			case TextureCompressionFormat::ASTC_6x6:
			case TextureCompressionFormat::ASTC_8x8:
				return 16; // ASTC always 16 bytes per block regardless of block size
				
			default:
				return 4; // RGBA8
		}
	}

	void TextureCompressor::GetBlockDimensions(TextureCompressionFormat format, uint32_t& width, uint32_t& height) {
		switch (format) {
			case TextureCompressionFormat::BC1:
			case TextureCompressionFormat::BC3:
			case TextureCompressionFormat::BC4:
			case TextureCompressionFormat::BC5:
			case TextureCompressionFormat::BC7:
			case TextureCompressionFormat::ETC1:
			case TextureCompressionFormat::ETC2_RGB:
			case TextureCompressionFormat::ETC2_RGBA:
			case TextureCompressionFormat::ASTC_4x4:
				width = 4;
				height = 4;
				break;
				
			case TextureCompressionFormat::ASTC_6x6:
				width = 6;
				height = 6;
				break;
				
			case TextureCompressionFormat::ASTC_8x8:
				width = 8;
				height = 8;
				break;
				
			default:
				width = 1;
				height = 1;
				break;
		}
	}

	bool TextureCompressor::IsFormatSupported(TextureCompressionFormat format) {
		// On desktop OpenGL 4.3+, most compressed formats are supported
		// For actual runtime checking, query with glGetInternalformativ or check extensions
		
		switch (format) {
			case TextureCompressionFormat::None:
				return true;
				
			case TextureCompressionFormat::BC1:
			case TextureCompressionFormat::BC3:
				// S3TC is universally supported on desktop GPUs
				return true;
				
			case TextureCompressionFormat::BC4:
			case TextureCompressionFormat::BC5:
				// RGTC is core in OpenGL 3.0+
				return true;
				
			case TextureCompressionFormat::BC7:
				// BPTC is core in OpenGL 4.2+
				return true;
				
			case TextureCompressionFormat::ETC2_RGB:
			case TextureCompressionFormat::ETC2_RGBA:
				// ETC2 is core in OpenGL 4.3+
				return true;
				
			case TextureCompressionFormat::ASTC_4x4:
			case TextureCompressionFormat::ASTC_6x6:
			case TextureCompressionFormat::ASTC_8x8:
				// ASTC requires extension - check at runtime
				// For now, return false on desktop (mainly mobile)
				return false;
				
			default:
				return false;
		}
	}

	const char* TextureCompressor::GetFormatName(TextureCompressionFormat format) {
		switch (format) {
			case TextureCompressionFormat::None: return "Uncompressed";
			case TextureCompressionFormat::BC1: return "BC1 (DXT1)";
			case TextureCompressionFormat::BC3: return "BC3 (DXT5)";
			case TextureCompressionFormat::BC4: return "BC4 (RGTC1)";
			case TextureCompressionFormat::BC5: return "BC5 (RGTC2)";
			case TextureCompressionFormat::BC7: return "BC7 (BPTC)";
			case TextureCompressionFormat::ETC1: return "ETC1";
			case TextureCompressionFormat::ETC2_RGB: return "ETC2 RGB";
			case TextureCompressionFormat::ETC2_RGBA: return "ETC2 RGBA";
			case TextureCompressionFormat::ASTC_4x4: return "ASTC 4x4";
			case TextureCompressionFormat::ASTC_6x6: return "ASTC 6x6";
			case TextureCompressionFormat::ASTC_8x8: return "ASTC 8x8";
			case TextureCompressionFormat::BasisUniversal: return "Basis Universal";
			default: return "Unknown";
		}
	}

	size_t TextureCompressor::CalculateCompressedSize(
		uint32_t width, 
		uint32_t height, 
		TextureCompressionFormat format
	) {
		if (format == TextureCompressionFormat::None) {
			return width * height * 4; // RGBA8
		}
		
		uint32_t blockW, blockH;
		GetBlockDimensions(format, blockW, blockH);
		
		uint32_t blocksX = (width + blockW - 1) / blockW;
		uint32_t blocksY = (height + blockH - 1) / blockH;
		
		return blocksX * blocksY * GetBytesPerBlock(format);
	}

} // namespace Lunex
