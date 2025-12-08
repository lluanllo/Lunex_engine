#include "stpch.h"
#include "OpenGLTexture.h"
#include "Renderer/TextureCompression.h"

#include "stb_image.h"

#include <glad/glad.h>

namespace Lunex {
	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height) : m_Width(width), m_Height(height) {
		LNX_PROFILE_FUNCTION();
		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;
		
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);
		
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	
	OpenGLTexture2D::OpenGLTexture2D(const std::string& path) : m_Path(path) {
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;  // Start as not loaded until we successfully load
		m_Width = 0;
		m_Height = 0;
		m_RendererID = 0;
		
		// ========================================================================
		// AUTO-COMPRESSION: Only attempt if KTX is actually available
		// ========================================================================
		auto& config = TextureCompressionConfig::Get();
		
		// Only try compression if KTX is available and auto-compression is enabled
		if (config.EnableAutoCompression && TextureCompressionConfig::IsKTXAvailable()) {
			TextureImportSettings settings = config.GetDefaultSettings();
			
			// Only compress if format is not None (meaning KTX determined compression is possible)
			if (settings.CompressionFormat != TextureCompressionFormat::None) {
				auto& compressor = TextureCompressor::Get();
				if (!compressor.IsInitialized()) {
					compressor.Initialize();
				}
				
				// Try to load from cache or compress
				CompressedTextureData compressedData = compressor.CompressFromFile(path, settings);
				if (compressedData.IsValid()) {
					LoadFromCompressedData(compressedData);
					return;
				}
				// If compression failed, fall through to standard loading
				LNX_LOG_TRACE("Compression failed for {0}, falling back to standard loading", path);
			}
		}
		
		// ========================================================================
		// STANDARD LOADING (no compression or compression failed)
		// ========================================================================
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = nullptr;
		{
			LNX_PROFILE_SCOPE("stbi_load - OpenGLTexture2D::OpenGLTexture2D(const std::string&)");
			data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		}
		
		if (data) {
			m_IsLoaded = true;
			
			m_Width = width;
			m_Height = height;
			
			GLenum internalFormat = 0, dataFormat = 0;
			if (channels == 4) {
				internalFormat = GL_RGBA8;
				dataFormat = GL_RGBA;
			}
			else if (channels == 3) {
				internalFormat = GL_RGB8;
				dataFormat = GL_RGB;
			}
			else if (channels == 1) {
				internalFormat = GL_R8;
				dataFormat = GL_RED;
			}
			
			m_InternalFormat = internalFormat;
			m_DataFormat = dataFormat;
			
			LNX_CORE_ASSERT(internalFormat && dataFormat, "Format not supported!");
			
			// Calculate mip levels for auto-generated mipmaps
			uint32_t mipLevels = 1;
			if (config.GenerateMipmaps) {
				mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
			}
			m_MipLevels = mipLevels;
			
			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, mipLevels, internalFormat, m_Width, m_Height);
			
			// Use LINEAR_MIPMAP_LINEAR if mipmaps are enabled
			if (mipLevels > 1) {
				glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			} else {
				glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			
			// Use CLAMP_TO_EDGE for textures with transparency to avoid border artifacts
			if (channels == 4) {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			} else {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}
			
			// Anisotropic filtering
			float maxAnisotropy;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
			glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.0f, maxAnisotropy));
			
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);
			
			// Generate mipmaps
			if (mipLevels > 1) {
				glGenerateTextureMipmap(m_RendererID);
			}
			
			stbi_image_free(data);
		}
		else {
			// Failed to load texture - log error and ensure state is clean
			LNX_LOG_ERROR("Failed to load texture: {0} - {1}", path, stbi_failure_reason());
			m_IsLoaded = false;
			m_Width = 0;
			m_Height = 0;
			m_RendererID = 0;
		}
	}
	
	OpenGLTexture2D::OpenGLTexture2D(const std::string& path, const TextureImportSettings& settings) 
		: m_Path(path) 
	{
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;  // Start as not loaded
		m_Width = 0;
		m_Height = 0;
		m_RendererID = 0;
		
		// Try to load from compressed cache first - only if KTX is available
		if (settings.UseCache && settings.CompressionFormat != TextureCompressionFormat::None 
			&& TextureCompressionConfig::IsKTXAvailable()) {
			auto& compressor = TextureCompressor::Get();
			if (!compressor.IsInitialized()) {
				compressor.Initialize();
			}
			
			CompressedTextureData compressedData = compressor.CompressFromFile(path, settings);
			if (compressedData.IsValid()) {
				LoadFromCompressedData(compressedData);
				return;
			}
		}
		
		// Fallback to standard loading
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		
		if (data) {
			m_IsLoaded = true;
			m_Width = width;
			m_Height = height;
			
			GLenum internalFormat = settings.IsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			GLenum dataFormat = GL_RGBA;
			
			if (channels == 4) {
				internalFormat = settings.IsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
				dataFormat = GL_RGBA;
			}
			else if (channels == 3) {
				internalFormat = settings.IsSRGB ? GL_SRGB8 : GL_RGB8;
				dataFormat = GL_RGB;
			}
			else if (channels == 1) {
				internalFormat = GL_R8;
				dataFormat = GL_RED;
			}
			
			m_InternalFormat = internalFormat;
			m_DataFormat = dataFormat;
			
			// Calculate mip levels
			uint32_t mipLevels = 1;
			if (settings.GenerateMipmaps) {
				mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
				if (settings.MaxMipLevels > 0) {
					mipLevels = std::min(mipLevels, static_cast<uint32_t>(settings.MaxMipLevels));
				}
			}
			m_MipLevels = mipLevels;
			
			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, mipLevels, internalFormat, m_Width, m_Height);
			
			// Filter settings
			if (settings.GenerateMipmaps) {
				glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			} else {
				glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			
			// Wrap mode
			if (channels == 4) {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			} else {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}
			
			// Anisotropic filtering
			float maxAnisotropy;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
			glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.0f, maxAnisotropy));
			
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);
			
			// Generate mipmaps
			if (settings.GenerateMipmaps) {
				glGenerateTextureMipmap(m_RendererID);
			}
			
			stbi_image_free(data);
		}
		else {
			// Failed to load texture
			LNX_LOG_ERROR("Failed to load texture: {0} - {1}", path, stbi_failure_reason());
			m_IsLoaded = false;
			m_Width = 0;
			m_Height = 0;
			m_RendererID = 0;
		}
	}
	
	OpenGLTexture2D::OpenGLTexture2D(const CompressedTextureData& compressedData) {
		LNX_PROFILE_FUNCTION();
		LoadFromCompressedData(compressedData);
	}
	
	void OpenGLTexture2D::LoadFromCompressedData(const CompressedTextureData& data) {
		if (!data.IsValid()) {
			LNX_LOG_ERROR("OpenGLTexture2D - Invalid compressed data");
			m_IsLoaded = false;
			return;
		}
		
		m_Width = data.Width;
		m_Height = data.Height;
		m_IsCompressed = data.Format != TextureCompressionFormat::None;
		m_CompressionFormat = data.Format;
		m_InternalFormat = data.InternalFormat;
		m_MipLevels = data.GetMipCount();
		m_IsLoaded = true;
		
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		
		if (m_IsCompressed) {
			// Allocate storage for compressed texture
			glTextureStorage2D(m_RendererID, m_MipLevels, m_InternalFormat, m_Width, m_Height);
			
			// Upload each mip level
			for (uint32_t i = 0; i < m_MipLevels; i++) {
				const uint8_t* mipData = data.GetMipData(i);
				size_t mipSize = data.GetMipDataSize(i);
				uint32_t mipWidth = data.MipLevels[i].Width;
				uint32_t mipHeight = data.MipLevels[i].Height;
				
				glCompressedTextureSubImage2D(
					m_RendererID,
					i,                    // mip level
					0, 0,                 // offset
					mipWidth, mipHeight,
					m_InternalFormat,
					static_cast<GLsizei>(mipSize),
					mipData
				);
			}
		}
		else {
			// Uncompressed path (from cache that stored raw data)
			m_DataFormat = GL_RGBA;
			glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data.Data.data());
		}
		
		// Filter settings
		if (m_MipLevels > 1) {
			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		} else {
			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
		
		// Anisotropic filtering
		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
		glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.0f, maxAnisotropy));
		
		LNX_LOG_TRACE("Loaded compressed texture: {0}x{1}, Format: {2}, Mips: {3}",
			m_Width, m_Height, 
			TextureCompressor::GetFormatName(m_CompressionFormat),
			m_MipLevels);
	}
	
	OpenGLTexture2D::~OpenGLTexture2D() {
		LNX_PROFILE_FUNCTION();
		glDeleteTextures(1, &m_RendererID);
	}
	
	void OpenGLTexture2D::SetData(void* data, uint32_t size) {
		LNX_PROFILE_FUNCTION();
		
		if (m_IsCompressed) {
			LNX_LOG_WARN("Cannot SetData on compressed texture");
			return;
		}
		
		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		LNX_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture!");
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
	}
	
	void OpenGLTexture2D::Bind(uint32_t slot) const {
		LNX_PROFILE_FUNCTION();
		glBindTextureUnit(slot, m_RendererID);
	}
}