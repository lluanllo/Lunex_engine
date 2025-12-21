#pragma once

/**
 * @file RHIFramebuffer.h
 * @brief Framebuffer and render target interfaces
 * 
 * Framebuffers are collections of render targets (color + depth/stencil)
 * that can be rendered to. This abstraction works across:
 * - OpenGL: FBO (Framebuffer Objects)
 * - Vulkan: VkFramebuffer + VkRenderPass
 * - DX12: Multiple RTVs + DSV
 */

#include "RHIResource.h"
#include "RHITypes.h"
#include "RHITexture.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// RENDER TARGET DESCRIPTION
	// ============================================================================
	
	/**
	 * @struct RenderTargetDesc
	 * @brief Description for a single render target attachment
	 */
	struct RenderTargetDesc {
		uint32_t Width = 0;
		uint32_t Height = 0;
		TextureFormat Format = TextureFormat::RGBA8;
		uint32_t SampleCount = 1;
		ClearValue ClearValue;
		
		// For existing texture attachment
		Ref<RHITexture2D> ExistingTexture;
		uint32_t MipLevel = 0;
		uint32_t ArrayLayer = 0;
		
		RenderTargetDesc() = default;
		
		RenderTargetDesc(uint32_t w, uint32_t h, TextureFormat fmt = TextureFormat::RGBA8)
			: Width(w), Height(h), Format(fmt) {}
		
		static RenderTargetDesc Color(uint32_t w, uint32_t h, TextureFormat fmt = TextureFormat::RGBA8) {
			return RenderTargetDesc(w, h, fmt);
		}
		
		static RenderTargetDesc Depth(uint32_t w, uint32_t h) {
			return RenderTargetDesc(w, h, TextureFormat::Depth24Stencil8);
		}
		
		static RenderTargetDesc FromTexture(Ref<RHITexture2D> texture, uint32_t mip = 0, uint32_t layer = 0) {
			RenderTargetDesc desc;
			desc.ExistingTexture = texture;
			desc.MipLevel = mip;
			desc.ArrayLayer = layer;
			if (texture) {
				desc.Width = texture->GetWidth() >> mip;
				desc.Height = texture->GetHeight() >> mip;
				desc.Format = texture->GetFormat();
			}
			return desc;
		}
	};

	// ============================================================================
	// FRAMEBUFFER DESCRIPTION
	// ============================================================================
	
	/**
	 * @struct FramebufferDesc
	 * @brief Complete description of a framebuffer
	 */
	struct FramebufferDesc {
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<RenderTargetDesc> ColorAttachments;
		RenderTargetDesc DepthAttachment;
		bool HasDepth = false;
		uint32_t SampleCount = 1;
		std::string DebugName;
		
		FramebufferDesc() = default;
		
		FramebufferDesc(uint32_t w, uint32_t h) : Width(w), Height(h) {}
		
		// Builder pattern
		FramebufferDesc& SetSize(uint32_t w, uint32_t h) {
			Width = w;
			Height = h;
			return *this;
		}
		
		FramebufferDesc& AddColorAttachment(TextureFormat format = TextureFormat::RGBA8) {
			RenderTargetDesc desc;
			desc.Width = Width;
			desc.Height = Height;
			desc.Format = format;
			desc.SampleCount = SampleCount;
			ColorAttachments.push_back(desc);
			return *this;
		}
		
		FramebufferDesc& AddColorAttachment(const RenderTargetDesc& desc) {
			ColorAttachments.push_back(desc);
			return *this;
		}
		
		FramebufferDesc& AddColorTexture(Ref<RHITexture2D> texture, uint32_t mip = 0) {
			ColorAttachments.push_back(RenderTargetDesc::FromTexture(texture, mip));
			return *this;
		}
		
		FramebufferDesc& SetDepthAttachment(TextureFormat format = TextureFormat::Depth24Stencil8) {
			DepthAttachment.Width = Width;
			DepthAttachment.Height = Height;
			DepthAttachment.Format = format;
			DepthAttachment.SampleCount = SampleCount;
			HasDepth = true;
			return *this;
		}
		
		FramebufferDesc& SetDepthTexture(Ref<RHITexture2D> texture, uint32_t mip = 0) {
			DepthAttachment = RenderTargetDesc::FromTexture(texture, mip);
			HasDepth = true;
			return *this;
		}
		
		FramebufferDesc& SetSampleCount(uint32_t samples) {
			SampleCount = samples;
			return *this;
		}
		
		FramebufferDesc& SetName(const std::string& name) {
			DebugName = name;
			return *this;
		}
	};

	// ============================================================================
	// LOAD/STORE OPERATIONS
	// ============================================================================
	
	enum class LoadOp : uint8_t {
		Load = 0,     // Preserve existing contents
		Clear,        // Clear to clear value
		DontCare      // Contents undefined (optimization)
	};

	enum class StoreOp : uint8_t {
		Store = 0,    // Preserve contents after pass
		DontCare      // Contents undefined after pass (optimization)
	};

	/**
	 * @struct AttachmentLoadStore
	 * @brief Load/store operations for an attachment
	 */
	struct AttachmentLoadStore {
		LoadOp LoadOperation = LoadOp::Clear;
		StoreOp StoreOperation = StoreOp::Store;
		LoadOp StencilLoadOp = LoadOp::DontCare;
		StoreOp StencilStoreOp = StoreOp::DontCare;
		ClearValue ClearValue;
	};

	// ============================================================================
	// RHI FRAMEBUFFER
	// ============================================================================
	
	/**
	 * @class RHIFramebuffer
	 * @brief Render target collection for off-screen rendering
	 * 
	 * Features:
	 * - Multiple color attachments
	 * - Depth/stencil attachment
	 * - MSAA support
	 * - Resize capability
	 * - Texture extraction
	 */
	class RHIFramebuffer : public RHIResource {
	public:
		virtual ~RHIFramebuffer() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Framebuffer; }
		
		// ============================================
		// FRAMEBUFFER INFO
		// ============================================
		
		/**
		 * @brief Get the framebuffer description
		 */
		const FramebufferDesc& GetDesc() const { return m_Desc; }
		
		/**
		 * @brief Get framebuffer width
		 */
		uint32_t GetWidth() const { return m_Desc.Width; }
		
		/**
		 * @brief Get framebuffer height
		 */
		uint32_t GetHeight() const { return m_Desc.Height; }
		
		/**
		 * @brief Get number of color attachments
		 */
		uint32_t GetColorAttachmentCount() const { 
			return static_cast<uint32_t>(m_Desc.ColorAttachments.size()); 
		}
		
		/**
		 * @brief Check if framebuffer has depth attachment
		 */
		bool HasDepthAttachment() const { return m_Desc.HasDepth; }
		
		/**
		 * @brief Get sample count
		 */
		uint32_t GetSampleCount() const { return m_Desc.SampleCount; }
		
		// ============================================
		// BINDING
		// ============================================
		
		/**
		 * @brief Bind framebuffer for rendering
		 */
		virtual void Bind() = 0;
		
		/**
		 * @brief Unbind framebuffer (bind default/backbuffer)
		 */
		virtual void Unbind() = 0;
		
		/**
		 * @brief Bind framebuffer for reading
		 */
		virtual void BindForRead() = 0;
		
		// ============================================
		// OPERATIONS
		// ============================================
		
		/**
		 * @brief Resize the framebuffer
		 * @param width New width
		 * @param height New height
		 */
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		
		/**
		 * @brief Clear all attachments
		 * @param colorValue Clear color for color attachments
		 * @param depth Clear depth value
		 * @param stencil Clear stencil value
		 */
		virtual void Clear(const ClearValue& colorValue, float depth = 1.0f, uint8_t stencil = 0) = 0;
		
		/**
		 * @brief Clear a specific color attachment
		 * @param attachmentIndex Attachment to clear
		 * @param value Clear value (as integer for RED_INTEGER, color for RGBA)
		 */
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
		
		/**
		 * @brief Clear depth/stencil attachment
		 */
		virtual void ClearDepth(float depth = 1.0f, uint8_t stencil = 0) = 0;
		
		// ============================================
		// TEXTURE ACCESS
		// ============================================
		
		/**
		 * @brief Get color attachment as texture
		 * @param index Attachment index
		 * @return Texture for the attachment
		 */
		virtual Ref<RHITexture2D> GetColorAttachment(uint32_t index = 0) const = 0;
		
		/**
		 * @brief Get depth attachment as texture
		 */
		virtual Ref<RHITexture2D> GetDepthAttachment() const = 0;
		
		/**
		 * @brief Get color attachment native ID (OpenGL texture ID)
		 */
		virtual RHIHandle GetColorAttachmentID(uint32_t index = 0) const = 0;
		
		/**
		 * @brief Get depth attachment native ID
		 */
		virtual RHIHandle GetDepthAttachmentID() const = 0;
		
		// ============================================
		// PIXEL READING
		// ============================================
		
		/**
		 * @brief Read a pixel value from an attachment
		 * @param attachmentIndex Which attachment to read
		 * @param x X coordinate
		 * @param y Y coordinate
		 * @return Pixel value as integer
		 */
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;
		
		/**
		 * @brief Read pixels to a buffer
		 * @param attachmentIndex Which attachment to read
		 * @param x Start X
		 * @param y Start Y
		 * @param width Width to read
		 * @param height Height to read
		 * @param buffer Destination buffer
		 */
		virtual void ReadPixels(uint32_t attachmentIndex, int x, int y, 
							   uint32_t width, uint32_t height, void* buffer) = 0;
		
		// ============================================
		// BLIT OPERATIONS
		// ============================================
		
		/**
		 * @brief Copy/resolve to another framebuffer
		 * @param dest Destination framebuffer
		 * @param filter Filtering mode for scaling
		 */
		virtual void BlitTo(RHIFramebuffer* dest, FilterMode filter = FilterMode::Linear) = 0;
		
		/**
		 * @brief Copy to default framebuffer (screen)
		 */
		virtual void BlitToScreen(uint32_t screenWidth, uint32_t screenHeight,
								  FilterMode filter = FilterMode::Linear) = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override;
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a framebuffer
		 * @param desc Framebuffer description
		 */
		static Ref<RHIFramebuffer> Create(const FramebufferDesc& desc);
		
		/**
		 * @brief Create a simple framebuffer with one color and optional depth
		 */
		static Ref<RHIFramebuffer> Create(uint32_t width, uint32_t height, 
										  TextureFormat colorFormat = TextureFormat::RGBA8,
										  bool withDepth = true);
		
	protected:
		FramebufferDesc m_Desc;
	};

	// ============================================================================
	// RENDER TARGET POOL
	// ============================================================================
	
	/**
	 * @class RenderTargetPool
	 * @brief Pool for reusing temporary render targets
	 * 
	 * Manages allocation and reuse of render targets within a frame.
	 * Targets are released back to the pool at frame end.
	 */
	class RenderTargetPool {
	public:
		static RenderTargetPool& Get();
		
		/**
		 * @brief Acquire a render target matching the description
		 * @param desc Required render target properties
		 * @return A suitable render target (may be recycled)
		 */
		Ref<RHIFramebuffer> Acquire(const FramebufferDesc& desc);
		
		/**
		 * @brief Release a render target back to the pool
		 * @param target Target to release
		 */
		void Release(Ref<RHIFramebuffer> target);
		
		/**
		 * @brief Called at end of frame to recycle released targets
		 */
		void OnFrameEnd();
		
		/**
		 * @brief Clear all pooled targets
		 */
		void Clear();
		
		/**
		 * @brief Get pool statistics
		 */
		size_t GetPooledCount() const { return m_AvailableTargets.size(); }
		size_t GetActiveCount() const { return m_ActiveTargets.size(); }
		
	private:
		RenderTargetPool() = default;
		
		bool MatchesDesc(const FramebufferDesc& a, const FramebufferDesc& b) const;
		
		std::vector<Ref<RHIFramebuffer>> m_AvailableTargets;
		std::vector<Ref<RHIFramebuffer>> m_ActiveTargets;
		std::vector<Ref<RHIFramebuffer>> m_ReleasedThisFrame;
	};

	// ============================================================================
	// SCOPED RENDER TARGET
	// ============================================================================
	
	/**
	 * @class ScopedRenderTarget
	 * @brief RAII wrapper for temporary render targets from the pool
	 */
	class ScopedRenderTarget {
	public:
		explicit ScopedRenderTarget(const FramebufferDesc& desc)
			: m_Target(RenderTargetPool::Get().Acquire(desc)) {}
		
		~ScopedRenderTarget() {
			if (m_Target) {
				RenderTargetPool::Get().Release(m_Target);
			}
		}
		
		// Move only
		ScopedRenderTarget(ScopedRenderTarget&& other) noexcept 
			: m_Target(std::move(other.m_Target)) {
			other.m_Target = nullptr;
		}
		
		ScopedRenderTarget& operator=(ScopedRenderTarget&& other) noexcept {
			if (this != &other) {
				if (m_Target) {
					RenderTargetPool::Get().Release(m_Target);
				}
				m_Target = std::move(other.m_Target);
				other.m_Target = nullptr;
			}
			return *this;
		}
		
		// No copy
		ScopedRenderTarget(const ScopedRenderTarget&) = delete;
		ScopedRenderTarget& operator=(const ScopedRenderTarget&) = delete;
		
		RHIFramebuffer* Get() const { return m_Target.get(); }
		RHIFramebuffer* operator->() const { return m_Target.get(); }
		operator bool() const { return m_Target != nullptr; }
		
	private:
		Ref<RHIFramebuffer> m_Target;
	};

} // namespace RHI
} // namespace Lunex
