#pragma once

#include "Renderer/TextureCube.h"
#include <glad/glad.h>
#include <array>

namespace Lunex {
	
	/**
	 * OpenGL implementation of TextureCube
	 * Supports:
	 *   - Loading from 6 individual face images
	 *   - Creating empty cubemaps for framebuffer rendering
	 *   - Converting equirectangular HDRIs to cubemaps
	 */
	class OpenGLTextureCube : public TextureCube {
	public:
		// Create from 6 face images
		OpenGLTextureCube(const std::array<std::string, 6>& facePaths);
		
		// Create empty cubemap
		OpenGLTextureCube(uint32_t size, bool hdr = true, uint32_t mipLevels = 0);
		
		virtual ~OpenGLTextureCube();
		
		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		
		virtual void Bind(uint32_t slot = 0) const override;
		virtual void Unbind() const override;
		
		virtual bool IsLoaded() const override { return m_IsLoaded; }
		virtual uint32_t GetMipLevelCount() const override { return m_MipLevels; }
		
		virtual bool operator==(const TextureCube& other) const override {
			return m_RendererID == other.GetRendererID();
		}
		
		// ========================================
		// HDRI CONVERSION
		// ========================================
		
		/**
		 * Create a cubemap from an equirectangular HDRI
		 * Uses a compute/render pass to convert panorama to cube faces
		 */
		static Ref<TextureCube> CreateFromHDRI(const std::string& hdriPath, uint32_t resolution = 1024);
		
		// ========================================
		// IBL GENERATION
		// ========================================
		
		// Generate diffuse irradiance map from this cubemap
		Ref<TextureCube> GenerateIrradianceMap(uint32_t resolution = 32) const;
		
		// Generate prefiltered specular map for split-sum approximation
		Ref<TextureCube> GeneratePrefilteredMap(uint32_t resolution = 128) const;
		
		// Generate mipmaps for this cubemap
		void GenerateMipmaps();
		
	private:
		void LoadFaces(const std::array<std::string, 6>& facePaths);
		void CreateEmpty(uint32_t size, bool hdr, uint32_t mipLevels);
		
	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_MipLevels = 1;
		GLenum m_InternalFormat = GL_RGBA16F;
		GLenum m_DataFormat = GL_RGBA;
		bool m_IsLoaded = false;
		bool m_IsHDR = true;
	};
	
}
