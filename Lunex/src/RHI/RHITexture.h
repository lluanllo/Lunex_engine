#pragma once

/**
 * @file RHITexture.h
 * @brief GPU texture interfaces for 2D, 3D, cube, and array textures
 * 
 * Provides abstract interfaces for all texture types with support for:
 * - Multiple formats (RGBA, depth, compressed)
 * - Mipmaps
 * - Texture arrays
 * - Render target usage
 * - Compute shader access
 */

#include "RHIResource.h"
#include "RHITypes.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// TEXTURE DESCRIPTION
	// ============================================================================
	
	struct TextureDesc {
		uint32_t Width = 1;
		uint32_t Height = 1;
		uint32_t Depth = 1;
		uint32_t ArrayLayers = 1;
		uint32_t MipLevels = 1;
		uint32_t SampleCount = 1;
		TextureFormat Format = TextureFormat::RGBA8;
		
		bool IsRenderTarget = false;
		bool IsStorage = false;
		bool GenerateMipmaps = false;
		
		TextureDesc() = default;
		
		TextureDesc(uint32_t width, uint32_t height, TextureFormat format = TextureFormat::RGBA8)
			: Width(width), Height(height), Format(format) {}
		
		static TextureDesc RenderTarget(uint32_t width, uint32_t height, TextureFormat format = TextureFormat::RGBA8) {
			TextureDesc desc(width, height, format);
			desc.IsRenderTarget = true;
			return desc;
		}
		
		static TextureDesc DepthTarget(uint32_t width, uint32_t height) {
			TextureDesc desc(width, height, TextureFormat::Depth24Stencil8);
			desc.IsRenderTarget = true;
			return desc;
		}
	};

	// ============================================================================
	// TEXTURE SUBRESOURCE
	// ============================================================================
	
	struct TextureSubresource {
		uint32_t MipLevel = 0;
		uint32_t ArrayLayer = 0;
		uint32_t MipCount = 1;
		uint32_t LayerCount = 1;
		
		static TextureSubresource All() {
			TextureSubresource sub;
			sub.MipCount = ~0u;
			sub.LayerCount = ~0u;
			return sub;
		}
	};

	struct TextureRegion {
		uint32_t X = 0;
		uint32_t Y = 0;
		uint32_t Z = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;
		uint32_t MipLevel = 0;
		uint32_t ArrayLayer = 0;
	};

	// ============================================================================
	// RHI TEXTURE BASE CLASS
	// ============================================================================
	
	/**
	 * @class RHITexture
	 * @brief Base class for all GPU textures
	 */
	class RHITexture : public RHIResource {
	public:
		virtual ~RHITexture() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Texture; }
		
		// ============================================
		// TEXTURE PROPERTIES
		// ============================================
		
		/**
		 * @brief Get the texture description
		 */
		const TextureDesc& GetDesc() const { return m_Desc; }
		
		/**
		 * @brief Get texture width
		 */
		uint32_t GetWidth() const { return m_Desc.Width; }
		
		/**
		 * @brief Get texture height
		 */
		uint32_t GetHeight() const { return m_Desc.Height; }
		
		/**
		 * @brief Get texture depth (for 3D textures)
		 */
		uint32_t GetDepth() const { return m_Desc.Depth; }
		
		/**
		 * @brief Get number of array layers
		 */
		uint32_t GetArrayLayers() const { return m_Desc.ArrayLayers; }
		
		/**
		 * @brief Get number of mip levels
		 */
		uint32_t GetMipLevels() const { return m_Desc.MipLevels; }
		
		/**
		 * @brief Get sample count (for MSAA)
		 */
		uint32_t GetSampleCount() const { return m_Desc.SampleCount; }
		
		/**
		 * @brief Get texture format
		 */
		TextureFormat GetFormat() const { return m_Desc.Format; }
		
		/**
		 * @brief Check if this is a render target
		 */
		bool IsRenderTarget() const { return m_Desc.IsRenderTarget; }
		
		/**
		 * @brief Check if this can be used as storage (compute)
		 */
		bool IsStorage() const { return m_Desc.IsStorage; }
		
		/**
		 * @brief Check if format is depth/stencil
		 */
		bool IsDepthFormat() const { return RHI::IsDepthFormat(m_Desc.Format); }
		
		/**
		 * @brief Check if format is compressed
		 */
		bool IsCompressed() const { return RHI::IsCompressedFormat(m_Desc.Format); }
		
		// ============================================
		// DATA OPERATIONS
		// ============================================
		
		/**
		 * @brief Upload data to the texture
		 * @param data Pixel data
		 * @param size Data size in bytes
		 * @param region Target region (default = full texture)
		 */
		virtual void SetData(const void* data, uint64_t size, const TextureRegion& region = {}) = 0;
		
		/**
		 * @brief Read data from the texture
		 * @param data Destination buffer
		 * @param size Buffer size
		 * @param region Source region
		 */
		virtual void GetData(void* data, uint64_t size, const TextureRegion& region = {}) const = 0;
		
		/**
		 * @brief Generate mipmaps
		 */
		virtual void GenerateMipmaps() = 0;
		
		// ============================================
		// BINDING
		// ============================================
		
		/**
		 * @brief Bind texture to a slot (OpenGL-style)
		 * @param slot Texture unit slot
		 */
		virtual void Bind(uint32_t slot = 0) const = 0;
		
		/**
		 * @brief Unbind texture from slot
		 */
		virtual void Unbind(uint32_t slot = 0) const = 0;
		
		/**
		 * @brief Bind for compute shader access
		 * @param slot Image unit slot
		 * @param access Read/write access mode
		 * @param mipLevel Mip level to bind
		 */
		virtual void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override {
			return CalculateTextureSize();
		}
		
	protected:
		uint64_t CalculateTextureSize() const;
		
		TextureDesc m_Desc;
	};

	// ============================================================================
	// RHI TEXTURE 2D
	// ============================================================================
	
	/**
	 * @class RHITexture2D
	 * @brief 2D texture interface
	 */
	class RHITexture2D : public RHITexture {
	public:
		virtual ~RHITexture2D() = default;
		
		/**
		 * @brief Resize the texture (only for render targets)
		 * @param width New width
		 * @param height New height
		 */
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		
		/**
		 * @brief Read a single pixel value (slow, for picking)
		 * @param x X coordinate
		 * @param y Y coordinate
		 * @return Pixel value as integer
		 */
		virtual int ReadPixel(int x, int y) const = 0;
		
		/**
		 * @brief Clear the texture to a value
		 * @param value Clear value
		 */
		virtual void Clear(const ClearValue& value) = 0;
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a 2D texture
		 * @param desc Texture description
		 * @return New texture or nullptr
		 */
		static Ref<RHITexture2D> Create(const TextureDesc& desc);
		
		/**
		 * @brief Create a 2D texture with initial data
		 * @param desc Texture description
		 * @param data Initial pixel data
		 * @param dataSize Data size in bytes
		 */
		static Ref<RHITexture2D> Create(const TextureDesc& desc, const void* data, uint64_t dataSize);
		
		/**
		 * @brief Load texture from file
		 * @param filePath Path to image file
		 * @param generateMips Whether to generate mipmaps
		 */
		static Ref<RHITexture2D> CreateFromFile(const std::string& filePath, bool generateMips = true);
	};

	// ============================================================================
	// RHI TEXTURE CUBE
	// ============================================================================
	
	/**
	 * @class RHITextureCube
	 * @brief Cubemap texture interface (6 faces)
	 */
	class RHITextureCube : public RHITexture {
	public:
		virtual ~RHITextureCube() = default;
		
		/**
		 * @brief Get face size (width = height for cube maps)
		 */
		uint32_t GetFaceSize() const { return GetWidth(); }
		
		/**
		 * @brief Set data for a specific face
		 * @param face Face index (0-5: +X, -X, +Y, -Y, +Z, -Z)
		 * @param data Pixel data
		 * @param size Data size
		 * @param mipLevel Mip level
		 */
		virtual void SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel = 0) = 0;
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a cube texture
		 * @param size Face size (width = height)
		 * @param format Texture format
		 * @param mipLevels Number of mip levels
		 */
		static Ref<RHITextureCube> Create(uint32_t size, TextureFormat format = TextureFormat::RGBA8, uint32_t mipLevels = 1);
		
		/**
		 * @brief Load cubemap from 6 face images
		 * @param facePaths Paths to 6 face images (+X, -X, +Y, -Y, +Z, -Z)
		 */
		static Ref<RHITextureCube> CreateFromFiles(const std::array<std::string, 6>& facePaths);
		
		/**
		 * @brief Load cubemap from a single equirectangular HDR image
		 * @param filePath Path to HDR image
		 * @param resolution Output cubemap resolution
		 */
		static Ref<RHITextureCube> CreateFromEquirectangular(const std::string& filePath, uint32_t resolution = 1024);
	};

	// ============================================================================
	// RHI TEXTURE 3D
	// ============================================================================
	
	/**
	 * @class RHITexture3D
	 * @brief 3D volume texture interface
	 */
	class RHITexture3D : public RHITexture {
	public:
		virtual ~RHITexture3D() = default;
		
		/**
		 * @brief Set data for a Z slice
		 * @param slice Z index
		 * @param data Pixel data for the slice
		 * @param size Data size
		 */
		virtual void SetSliceData(uint32_t slice, const void* data, uint64_t size) = 0;
		
		// ============================================
		// FACTORY
		// ============================================
		
		static Ref<RHITexture3D> Create(uint32_t width, uint32_t height, uint32_t depth, 
										TextureFormat format = TextureFormat::RGBA8);
	};

	// ============================================================================
	// RHI TEXTURE ARRAY
	// ============================================================================
	
	/**
	 * @class RHITexture2DArray
	 * @brief 2D texture array interface
	 */
	class RHITexture2DArray : public RHITexture {
	public:
		virtual ~RHITexture2DArray() = default;
		
		/**
		 * @brief Set data for a specific layer
		 * @param layer Array layer index
		 * @param data Pixel data
		 * @param size Data size
		 * @param mipLevel Mip level
		 */
		virtual void SetLayerData(uint32_t layer, const void* data, uint64_t size, uint32_t mipLevel = 0) = 0;
		
		// ============================================
		// FACTORY
		// ============================================
		
		static Ref<RHITexture2DArray> Create(uint32_t width, uint32_t height, uint32_t layers,
											 TextureFormat format = TextureFormat::RGBA8, uint32_t mipLevels = 1);
	};

} // namespace RHI
} // namespace Lunex
