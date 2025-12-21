#pragma once

#include "Core/Core.h"
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace Lunex {

	// Forward declarations
	struct TextureImportSettings;

	// ============================================================================
	// COMPRESSED TEXTURE FORMATS
	// Supports GPU-native compressed formats via KTX2
	// ============================================================================
	
	enum class TextureCompressionFormat : uint8_t {
		None = 0,           // Uncompressed RGBA8/RGB8
		
		// Desktop formats (BC/DXT)
		BC1,                // DXT1 - RGB, 1-bit alpha (4 bpp)
		BC3,                // DXT5 - RGBA with interpolated alpha (8 bpp)
		BC4,                // Single channel (grayscale/height maps) (4 bpp)
		BC5,                // Two channels (normal maps) (8 bpp)
		BC7,                // High quality RGBA (8 bpp)
		
		// Mobile formats
		ETC1,               // RGB only (4 bpp) - legacy Android
		ETC2_RGB,           // RGB (4 bpp) - OpenGL ES 3.0+
		ETC2_RGBA,          // RGBA (8 bpp) - OpenGL ES 3.0+
		
		// Universal format
		ASTC_4x4,           // High quality (8 bpp)
		ASTC_6x6,           // Medium quality (3.56 bpp)
		ASTC_8x8,           // Lower quality (2 bpp)
		
		// Basis Universal (transcodes to native format at runtime)
		BasisUniversal,     // Auto-selects best format for platform
		
		Count
	};

	// ============================================================================
	// TEXTURE IMPORT SETTINGS
	// Configuration for how textures should be processed
	// ============================================================================
	
	struct TextureImportSettings {
		// Compression
		TextureCompressionFormat CompressionFormat = TextureCompressionFormat::BC7;
		int CompressionQuality = 128;         // 0-255, higher = better quality, slower
		bool GenerateMipmaps = true;
		int MaxMipLevels = 0;                 // 0 = auto (full chain)
		
		// Size constraints
		uint32_t MaxWidth = 4096;
		uint32_t MaxHeight = 4096;
		bool PowerOfTwo = false;              // Force power-of-two dimensions
		
		// Color space
		bool IsSRGB = true;                   // Use sRGB color space
		bool IsNormalMap = false;             // Optimize for normal maps (BC5, linear)
		bool IsHDR = false;                   // High dynamic range texture
		
		// Alpha handling
		bool PreserveAlpha = true;
		bool PremultiplyAlpha = false;
		
		// Platform targets (for Basis Universal)
		bool TargetDesktop = true;
		bool TargetMobile = false;
		bool TargetWeb = false;
		
		// Cache settings
		bool UseCache = true;                 // Use cached compressed version if available
		bool ForceRecompress = false;         // Ignore cache and recompress
		
		// Get recommended format based on settings
		TextureCompressionFormat GetRecommendedFormat() const;
		
		// Serialize/deserialize for .meta files
		std::string Serialize() const;
		static TextureImportSettings Deserialize(const std::string& data);
		
		// Presets
		static TextureImportSettings Default();
		static TextureImportSettings NormalMap();
		static TextureImportSettings HDR();
		static TextureImportSettings UI();
		static TextureImportSettings Sprite();
	};

	// ============================================================================
	// GLOBAL TEXTURE COMPRESSION CONFIG
	// Controls automatic compression behavior
	// ============================================================================
	
	struct TextureCompressionConfig {
		// Enable automatic compression for all textures loaded with Create(path)
		// NOTE: Only effective if KTX-Software is installed
		bool EnableAutoCompression = false;  // Disabled by default - enable only when KTX is installed
		
		// Default compression format (BC7 recommended for desktop)
		TextureCompressionFormat DefaultFormat = TextureCompressionFormat::BC7;
		
		// Quality settings
		int DefaultQuality = 128;           // 0-255
		bool GenerateMipmaps = true;
		
		// Cache settings
		bool UseCache = true;               // Cache compressed textures to disk
		std::filesystem::path CacheDirectory = ".texture_cache";
		
		// Singleton access
		static TextureCompressionConfig& Get() {
			static TextureCompressionConfig instance;
			return instance;
		}
		
		// Check if KTX compression is available
		static bool IsKTXAvailable() {
#ifdef LNX_HAS_KTX
			return true;
#else
			return false;
#endif
		}
		
		// Get default import settings based on config
		TextureImportSettings GetDefaultSettings() const;
	};

	// ============================================================================
	// COMPRESSED TEXTURE DATA
	// Container for compressed texture data ready for GPU upload
	// ============================================================================
	
	struct CompressedMipLevel {
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t DataSize = 0;
		uint32_t DataOffset = 0;           // Offset into the data buffer
	};

	struct CompressedTextureData {
		// Dimensions
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;                // For 3D textures
		uint32_t ArraySize = 1;            // For texture arrays
		
		// Format info
		TextureCompressionFormat Format = TextureCompressionFormat::None;
		uint32_t InternalFormat = 0;       // OpenGL internal format
		bool IsSRGB = true;
		bool IsCubemap = false;
		
		// Mipmap info
		std::vector<CompressedMipLevel> MipLevels;
		
		// Raw compressed data
		std::vector<uint8_t> Data;
		
		// Utility methods
		bool IsValid() const { return !Data.empty() && Width > 0 && Height > 0; }
		size_t GetTotalSize() const { return Data.size(); }
		uint32_t GetMipCount() const { return static_cast<uint32_t>(MipLevels.size()); }
		
		// Get data for specific mip level
		const uint8_t* GetMipData(uint32_t level) const;
		size_t GetMipDataSize(uint32_t level) const;
	};

	// ============================================================================
	// TEXTURE COMPRESSOR
	// Handles compression and KTX2 file operations
	// ============================================================================
	
	class TextureCompressor {
	public:
		// Singleton access
		static TextureCompressor& Get();
		
		// Initialize/shutdown
		void Initialize();
		void Shutdown();
		bool IsInitialized() const { return m_Initialized; }
		
		// ========== COMPRESSION ==========
		
		// Compress raw RGBA data to specified format
		CompressedTextureData Compress(
			const uint8_t* data,
			uint32_t width,
			uint32_t height,
			uint32_t channels,
			const TextureImportSettings& settings
		);
		
		// Compress from file
		CompressedTextureData CompressFromFile(
			const std::filesystem::path& sourcePath,
			const TextureImportSettings& settings
		);
		
		// ========== KTX2 FILE OPERATIONS ==========
		
		// Save compressed data to KTX2 file
		bool SaveToKTX2(
			const CompressedTextureData& data,
			const std::filesystem::path& outputPath
		);
		
		// Load from KTX2 file
		CompressedTextureData LoadFromKTX2(
			const std::filesystem::path& ktxPath
		);
		
		// ========== CACHE OPERATIONS ==========
		
		// Get cached KTX2 path for a source texture
		std::filesystem::path GetCachePath(const std::filesystem::path& sourcePath) const;
		
		// Check if cache is valid (source hasn't been modified)
		bool IsCacheValid(const std::filesystem::path& sourcePath) const;
		
		// Clear texture cache
		void ClearCache();
		void ClearCacheForFile(const std::filesystem::path& sourcePath);
		
		// Set cache directory (default: project/.texture_cache/)
		void SetCacheDirectory(const std::filesystem::path& path);
		const std::filesystem::path& GetCacheDirectory() const { return m_CacheDirectory; }
		
		// ========== UTILITIES ==========
		
		// Get OpenGL internal format for compression format
		static uint32_t GetGLInternalFormat(TextureCompressionFormat format, bool sRGB);
		
		// Get bytes per block for format
		static uint32_t GetBytesPerBlock(TextureCompressionFormat format);
		
		// Get block dimensions
		static void GetBlockDimensions(TextureCompressionFormat format, uint32_t& width, uint32_t& height);
		
		// Check if format is supported on current GPU
		static bool IsFormatSupported(TextureCompressionFormat format);
		
		// Get format name for display
		static const char* GetFormatName(TextureCompressionFormat format);
		
		// Calculate compressed size
		static size_t CalculateCompressedSize(
			uint32_t width, 
			uint32_t height, 
			TextureCompressionFormat format
		);
		
	private:
		TextureCompressor() = default;
		~TextureCompressor() = default;
		
		// Non-copyable
		TextureCompressor(const TextureCompressor&) = delete;
		TextureCompressor& operator=(const TextureCompressor&) = delete;
		
		// Internal helpers
		CompressedTextureData CompressWithKTX(
			const uint8_t* data,
			uint32_t width,
			uint32_t height,
			uint32_t channels,
			const TextureImportSettings& settings
		);
		
		void GenerateMipmaps(
			std::vector<uint8_t>& data,
			uint32_t& width,
			uint32_t& height,
			uint32_t channels
		);
		
	private:
		bool m_Initialized = false;
		std::filesystem::path m_CacheDirectory;
	};

} // namespace Lunex
