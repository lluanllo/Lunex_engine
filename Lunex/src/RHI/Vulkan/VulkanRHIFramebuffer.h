/**
 * @file VulkanRHIFramebuffer.h
 * @brief Vulkan implementation of RHI framebuffer (VkFramebuffer + VkRenderPass)
 */

#pragma once

#include "RHI/RHIFramebuffer.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHIDevice;

	// ============================================================================
	// VULKAN RHI FRAMEBUFFER
	// ============================================================================

	class VulkanRHIFramebuffer : public RHIFramebuffer {
	public:
		VulkanRHIFramebuffer(VulkanRHIDevice* device, const FramebufferDesc& desc);
		virtual ~VulkanRHIFramebuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Framebuffer); }
		bool IsValid() const override { return m_Framebuffer != VK_NULL_HANDLE; }

		// Binding
		void Bind() override;
		void Unbind() override;
		void BindForRead() override;

		// Operations
		void Resize(uint32_t width, uint32_t height) override;
		void Clear(const ClearValue& colorValue, float depth = 1.0f, uint8_t stencil = 0) override;
		void ClearAttachment(uint32_t attachmentIndex, int value) override;
		void ClearDepth(float depth = 1.0f, uint8_t stencil = 0) override;

		// Texture access
		Ref<RHITexture2D> GetColorAttachment(uint32_t index = 0) const override;
		Ref<RHITexture2D> GetDepthAttachment() const override;
		RHIHandle GetColorAttachmentID(uint32_t index = 0) const override;
		RHIHandle GetDepthAttachmentID() const override;

		// Pixel reading
		int ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		void ReadPixels(uint32_t attachmentIndex, int x, int y,
					   uint32_t width, uint32_t height, void* buffer) override;

		// Blit operations
		void BlitTo(RHIFramebuffer* dest, FilterMode filter = FilterMode::Linear) override;
		void BlitToScreen(uint32_t screenWidth, uint32_t screenHeight,
						 FilterMode filter = FilterMode::Linear) override;

		// Vulkan specific
		VkFramebuffer GetVkFramebuffer() const { return m_Framebuffer; }
		VkRenderPass GetVkRenderPass() const { return m_RenderPass; }

	protected:
		void OnDebugNameChanged() override;

	private:
		void CreateFramebuffer();
		void DestroyFramebuffer();

		VulkanRHIDevice* m_Device = nullptr;
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;

		std::vector<Ref<RHITexture2D>> m_ColorAttachments;
		Ref<RHITexture2D> m_DepthAttachment;
	};

} // namespace RHI
} // namespace Lunex
