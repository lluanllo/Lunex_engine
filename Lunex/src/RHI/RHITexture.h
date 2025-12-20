#pragma once

/**
 * @file RHITexture.h
 * @brief GPU texture interfaces for 2D, 3D, cube, and array textures
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
		
		std::string DebugName;
		
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
		
		const TextureDesc& GetDesc() const { return m_Desc; }
		uint32_t GetWidth() const { return m_Desc.Width; }
		uint32_t GetHeight() const { return m_Desc.Height; }
		uint32_t GetDepth() const { return m_Desc.Depth; }
		uint32_t GetArrayLayers() const { return m_Desc.ArrayLayers; }
		uint32_t GetMipLevels() const { return m_Desc.MipLevels; }
		uint32_t GetSampleCount() const { return m_Desc.SampleCount; }
		TextureFormat GetFormat() const { return m_Desc.Format; }
		bool IsRenderTarget() const { return m_Desc.IsRenderTarget; }
		bool IsStorage() const { return m_Desc.IsStorage; }
		bool IsDepthFormat() const { return RHI::IsDepthFormat(m_Desc.Format); }
		bool IsCompressed() const { return RHI::IsCompressedFormat(m_Desc.Format); }
		
		// ============================================
		// DATA OPERATIONS
		// ============================================
		
		virtual void SetData(const void* data, uint64_t size, const TextureRegion& region) = 0;
		virtual void GetData(void* data, uint64_t size, const TextureRegion& region) const = 0;
		virtual void GenerateMipmaps() = 0;
		
		// Convenience overloads without region (uses full texture)
		void SetData(const void* data, uint64_t size) {
			SetData(data, size, TextureRegion{});
		}
		void GetData(void* data, uint64_t size) const {
			GetData(data, size, TextureRegion{});
		}
		
		// ============================================
		// BINDING
		// ============================================
		
		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind(uint32_t slot = 0) const = 0;
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
	
	class RHITexture2D : public RHITexture {
	public:
		virtual ~RHITexture2D() = default;
		
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual int ReadPixel(int x, int y) const = 0;
		virtual void Clear(const ClearValue& value) = 0;
		
		// Factory
		static Ref<RHITexture2D> Create(const TextureDesc& desc);
		static Ref<RHITexture2D> Create(const TextureDesc& desc, const void* data, uint64_t dataSize);
		static Ref<RHITexture2D> CreateFromFile(const std::string& filePath, bool generateMips = true);
	};

	// ============================================================================
	// RHI TEXTURE CUBE
	// ============================================================================
	
	class RHITextureCube : public RHITexture {
	public:
		virtual ~RHITextureCube() = default;
		
		uint32_t GetFaceSize() const { return GetWidth(); }
		virtual void SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel = 0) = 0;
		
		// Factory
		static Ref<RHITextureCube> Create(uint32_t size, TextureFormat format = TextureFormat::RGBA8, uint32_t mipLevels = 1);
		static Ref<RHITextureCube> CreateFromFiles(const std::array<std::string, 6>& facePaths);
		static Ref<RHITextureCube> CreateFromEquirectangular(const std::string& filePath, uint32_t resolution = 1024);
	};

	// ============================================================================
	// RHI TEXTURE 3D
	// ============================================================================
	
	class RHITexture3D : public RHITexture {
	public:
		virtual ~RHITexture3D() = default;
		virtual void SetSliceData(uint32_t slice, const void* data, uint64_t size) = 0;
		
		static Ref<RHITexture3D> Create(uint32_t width, uint32_t height, uint32_t depth, 
										TextureFormat format = TextureFormat::RGBA8);
	};

	// ============================================================================
	// RHI TEXTURE ARRAY
	// ============================================================================
	
	class RHITexture2DArray : public RHITexture {
	public:
		virtual ~RHITexture2DArray() = default;
		virtual void SetLayerData(uint32_t layer, const void* data, uint64_t size, uint32_t mipLevel = 0) = 0;
		
		static Ref<RHITexture2DArray> Create(uint32_t width, uint32_t height, uint32_t layers,
											 TextureFormat format = TextureFormat::RGBA8, uint32_t mipLevels = 1);
	};

} // namespace RHI
} // namespace Lunex
