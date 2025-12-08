#pragma once

#include <string>

#include "Core/Core.h"
#include "TextureCompression.h"

namespace Lunex {
	
	// Forward declaration
	struct CompressedTextureData;
	struct TextureImportSettings;
	
	class Texture {
		public:
			virtual ~Texture() = default;
			
			virtual uint32_t GetWidth() const = 0;
			virtual uint32_t GetHeight() const = 0;
			virtual uint32_t GetRendererID() const = 0;
			
			virtual const std::string& GetPath() const = 0;
			
			virtual void SetData(void* data, uint32_t size) = 0;
			
			virtual void Bind(uint32_t slot = 0) const = 0;
			
			virtual bool IsLoaded() const = 0;
			
			// ========== COMPRESSION SUPPORT ==========
			
			virtual bool IsCompressed() const = 0;
			virtual TextureCompressionFormat GetCompressionFormat() const = 0;
			virtual uint32_t GetMipLevelCount() const = 0;
			
			virtual bool operator==(const Texture& other) const = 0;
	};
	
	class Texture2D : public Texture {
		public:
			// Create empty texture
			static Ref<Texture2D> Create(uint32_t width, uint32_t height);
			
			// Create from file (uses cache if available)
			static Ref<Texture2D> Create(const std::string& path);
			
			// Create from file with explicit settings
			static Ref<Texture2D> Create(const std::string& path, const TextureImportSettings& settings);
			
			// Create from pre-compressed data (e.g., from KTX2 file)
			static Ref<Texture2D> CreateCompressed(const CompressedTextureData& compressedData);
	};
}