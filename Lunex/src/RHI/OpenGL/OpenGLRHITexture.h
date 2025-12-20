#pragma once

/**
 * @file OpenGLRHITexture.h
 * @brief OpenGL implementation of RHI texture types
 */

#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include <glad/glad.h>

// ============================================================================
// FALLBACK DEFINES FOR MISSING GLAD EXTENSIONS
// ============================================================================

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#endif

#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif

#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#endif

#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif

#ifndef GL_COMPRESSED_RG_RGTC2
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#endif

#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif

#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL TEXTURE UTILITIES
	// ============================================================================
	
	namespace OpenGLTextureUtils {
		
		// Get OpenGL internal format from RHI format
		inline GLenum GetInternalFormat(TextureFormat format) {
			switch (format) {
				// 8-bit
				case TextureFormat::R8:            return GL_R8;
				case TextureFormat::RG8:           return GL_RG8;
				case TextureFormat::RGB8:          return GL_RGB8;
				case TextureFormat::RGBA8:         return GL_RGBA8;
				case TextureFormat::SRGB8:         return GL_SRGB8;
				case TextureFormat::SRGBA8:        return GL_SRGB8_ALPHA8;
				
				// 16-bit float
				case TextureFormat::R16F:          return GL_R16F;
				case TextureFormat::RG16F:         return GL_RG16F;
				case TextureFormat::RGB16F:        return GL_RGB16F;
				case TextureFormat::RGBA16F:       return GL_RGBA16F;
				
				// 32-bit float
				case TextureFormat::R32F:          return GL_R32F;
				case TextureFormat::RG32F:         return GL_RG32F;
				case TextureFormat::RGB32F:        return GL_RGB32F;
				case TextureFormat::RGBA32F:       return GL_RGBA32F;
				
				// Integer
				case TextureFormat::R32I:          return GL_R32I;
				case TextureFormat::RG32I:         return GL_RG32I;
				case TextureFormat::RGBA32I:       return GL_RGBA32I;
				case TextureFormat::R32UI:         return GL_R32UI;
				
				// Depth/Stencil
				case TextureFormat::Depth16:       return GL_DEPTH_COMPONENT16;
				case TextureFormat::Depth24:       return GL_DEPTH_COMPONENT24;
				case TextureFormat::Depth32F:      return GL_DEPTH_COMPONENT32F;
				case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
				case TextureFormat::Depth32FStencil8: return GL_DEPTH32F_STENCIL8;
				
				// Compressed
				case TextureFormat::BC1:           return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				case TextureFormat::BC1_SRGB:      return GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
				case TextureFormat::BC3:           return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				case TextureFormat::BC3_SRGB:      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
				case TextureFormat::BC4:           return GL_COMPRESSED_RED_RGTC1;
				case TextureFormat::BC5:           return GL_COMPRESSED_RG_RGTC2;
				case TextureFormat::BC7:           return GL_COMPRESSED_RGBA_BPTC_UNORM;
				case TextureFormat::BC7_SRGB:      return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
				
				default: return GL_RGBA8;
			}
		}
		
		// Get OpenGL format (data format)
		inline GLenum GetFormat(TextureFormat format) {
			switch (format) {
				case TextureFormat::R8:
				case TextureFormat::R16F:
				case TextureFormat::R32F:
					return GL_RED;
					
				case TextureFormat::RG8:
				case TextureFormat::RG16F:
				case TextureFormat::RG32F:
					return GL_RG;
					
				case TextureFormat::RGB8:
				case TextureFormat::RGB16F:
				case TextureFormat::RGB32F:
				case TextureFormat::SRGB8:
					return GL_RGB;
					
				case TextureFormat::RGBA8:
				case TextureFormat::RGBA16F:
				case TextureFormat::RGBA32F:
				case TextureFormat::SRGBA8:
					return GL_RGBA;
					
				case TextureFormat::R32I:
					return GL_RED_INTEGER;
				case TextureFormat::RG32I:
					return GL_RG_INTEGER;
				case TextureFormat::RGBA32I:
					return GL_RGBA_INTEGER;
				case TextureFormat::R32UI:
					return GL_RED_INTEGER;
					
				case TextureFormat::Depth16:
				case TextureFormat::Depth24:
				case TextureFormat::Depth32F:
					return GL_DEPTH_COMPONENT;
					
				case TextureFormat::Depth24Stencil8:
				case TextureFormat::Depth32FStencil8:
					return GL_DEPTH_STENCIL;
					
				default: return GL_RGBA;
			}
		}
		
		// Get OpenGL type
		inline GLenum GetType(TextureFormat format) {
			switch (format) {
				case TextureFormat::R8:
				case TextureFormat::RG8:
				case TextureFormat::RGB8:
				case TextureFormat::RGBA8:
				case TextureFormat::SRGB8:
				case TextureFormat::SRGBA8:
					return GL_UNSIGNED_BYTE;
					
				case TextureFormat::R16F:
				case TextureFormat::RG16F:
				case TextureFormat::RGB16F:
				case TextureFormat::RGBA16F:
					return GL_HALF_FLOAT;
					
				case TextureFormat::R32F:
				case TextureFormat::RG32F:
				case TextureFormat::RGB32F:
				case TextureFormat::RGBA32F:
				case TextureFormat::Depth32F:
					return GL_FLOAT;
					
				case TextureFormat::R32I:
				case TextureFormat::RG32I:
				case TextureFormat::RGBA32I:
					return GL_INT;
					
				case TextureFormat::R32UI:
					return GL_UNSIGNED_INT;
					
				case TextureFormat::Depth16:
					return GL_UNSIGNED_SHORT;
					
				case TextureFormat::Depth24:
					return GL_UNSIGNED_INT;
					
				case TextureFormat::Depth24Stencil8:
					return GL_UNSIGNED_INT_24_8;
					
				case TextureFormat::Depth32FStencil8:
					return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
					
				default: return GL_UNSIGNED_BYTE;
			}
		}
		
		// Calculate mip count
		inline uint32_t CalculateMipCount(uint32_t width, uint32_t height) {
			return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}
		
	} // namespace OpenGLTextureUtils

	// ============================================================================
	// OPENGL RHI TEXTURE 2D
	// ============================================================================
	
	class OpenGLRHITexture2D : public RHITexture2D {
	public:
		OpenGLRHITexture2D(const TextureDesc& desc, const void* initialData = nullptr);
		virtual ~OpenGLRHITexture2D();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_TextureID); }
		bool IsValid() const override { return m_TextureID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;
		
		// Binding
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;
		
		// Texture2D specific
		void Resize(uint32_t width, uint32_t height) override;
		int ReadPixel(int x, int y) const override;
		void Clear(const ClearValue& value) override;
		
		// OpenGL specific
		GLuint GetTextureID() const { return m_TextureID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		void CreateTexture(const void* data);
		
		GLuint m_TextureID = 0;
		GLenum m_InternalFormat = GL_RGBA8;
	};

	// ============================================================================
	// OPENGL RHI TEXTURE CUBE
	// ============================================================================
	
	class OpenGLRHITextureCube : public RHITextureCube {
	public:
		OpenGLRHITextureCube(const TextureDesc& desc);
		virtual ~OpenGLRHITextureCube();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_TextureID); }
		bool IsValid() const override { return m_TextureID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;
		void SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel = 0) override;
		
		// Binding
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;
		
		// OpenGL specific
		GLuint GetTextureID() const { return m_TextureID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_TextureID = 0;
		GLenum m_InternalFormat = GL_RGBA8;
	};

	// ============================================================================
	// OPENGL RHI SAMPLER
	// ============================================================================
	
	class OpenGLRHISampler : public RHISampler {
	public:
		OpenGLRHISampler(const SamplerState& state);
		virtual ~OpenGLRHISampler();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_SamplerID); }
		bool IsValid() const override { return m_SamplerID != 0; }
		
		// Binding
		void Bind(uint32_t slot) const override;
		void Unbind(uint32_t slot) const override;
		
		// OpenGL specific
		GLuint GetSamplerID() const { return m_SamplerID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_SamplerID = 0;
	};

} // namespace RHI
} // namespace Lunex
