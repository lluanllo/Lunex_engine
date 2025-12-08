#pragma once

#include "Renderer/Texture.h"
#include "Renderer/TextureCompression.h"

#include <glad/glad.h>

namespace Lunex {
	class OpenGLTexture2D : public Texture2D {
		public:
			OpenGLTexture2D(uint32_t width, uint32_t height);
			OpenGLTexture2D(const std::string& path);
			OpenGLTexture2D(const std::string& path, const TextureImportSettings& settings);
			OpenGLTexture2D(const CompressedTextureData& compressedData);
			virtual ~OpenGLTexture2D();
			
			virtual uint32_t GetWidth() const override { return m_Width; };
			virtual uint32_t GetHeight() const override { return m_Height;  };
			virtual uint32_t GetRendererID() const override { return m_RendererID; };
			
			virtual const std::string& GetPath() const override { return m_Path; }
			
			virtual void SetData(void* data, uint32_t size) override;
			
			virtual void Bind(uint32_t slot = 0) const override;
			
			virtual bool IsLoaded() const override { return m_IsLoaded; }
			
			// Compression support
			virtual bool IsCompressed() const override { return m_IsCompressed; }
			virtual TextureCompressionFormat GetCompressionFormat() const override { return m_CompressionFormat; }
			virtual uint32_t GetMipLevelCount() const override { return m_MipLevels; }
			
			virtual bool operator==(const Texture& other) const override {
				return m_RendererID == other.GetRendererID();
			}
			
		private:
			void LoadFromCompressedData(const CompressedTextureData& data);
			
		private:
			std::string m_Path;
			bool m_IsLoaded = false;  // Changed from true to false - textures start as not loaded
			uint32_t m_Width, m_Height;
			uint32_t m_RendererID;
			GLenum m_InternalFormat, m_DataFormat;
			
			// Compression info
			bool m_IsCompressed = false;
			TextureCompressionFormat m_CompressionFormat = TextureCompressionFormat::None;
			uint32_t m_MipLevels = 1;
	};
}