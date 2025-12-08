#pragma once

#include "Core/Core.h"
#include <string>
#include <vector>
#include <array>

namespace Lunex {
	
	/**
	 * TextureCube - Abstract interface for Cubemap textures
	 * 
	 * A cubemap texture is a 6-sided texture that can be indexed using a 3D direction vector.
	 * Used for:
	 *   - Environment mapping (skyboxes)
	 *   - Image Based Lighting (IBL)
	 *   - Reflection probes
	 */
	class TextureCube {
	public:
		virtual ~TextureCube() = default;
		
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;
		
		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind() const = 0;
		
		virtual bool IsLoaded() const = 0;
		
		// Get the number of mip levels
		virtual uint32_t GetMipLevelCount() const = 0;
		
		virtual bool operator==(const TextureCube& other) const = 0;
		
		// ========================================
		// FACTORY METHODS
		// ========================================
		
		/**
		 * Create a cubemap from 6 individual face images
		 * Faces order: +X, -X, +Y, -Y, +Z, -Z
		 */
		static Ref<TextureCube> Create(const std::array<std::string, 6>& facePaths);
		
		/**
		 * Create an empty cubemap for rendering (e.g., for HDRI conversion)
		 * @param size The width/height of each face (must be square)
		 * @param hdr Whether to use HDR format (GL_RGBA16F)
		 * @param mipLevels Number of mip levels (0 = auto-calculate)
		 */
		static Ref<TextureCube> Create(uint32_t size, bool hdr = true, uint32_t mipLevels = 0);
		
		/**
		 * Create a cubemap from an equirectangular HDRI image
		 * This will convert the panorama to a cubemap format
		 */
		static Ref<TextureCube> CreateFromHDRI(const std::string& hdriPath, uint32_t resolution = 1024);
	};
	
}
