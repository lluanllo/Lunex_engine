/**
 * @file VulkanRHIFramebuffer.cpp
 * @brief Vulkan implementation of RHI framebuffer
 */

#include "stpch.h"
#include "VulkanRHIFramebuffer.h"
#include "VulkanRHITexture.h"
#include "VulkanRHIDevice.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	VulkanRHIFramebuffer::VulkanRHIFramebuffer(VulkanRHIDevice* device, const FramebufferDesc& desc)
		: m_Device(device)
	{
		m_Desc = desc;
		CreateFramebuffer();
	}

	VulkanRHIFramebuffer::~VulkanRHIFramebuffer() {
		DestroyFramebuffer();
	}

	void VulkanRHIFramebuffer::CreateFramebuffer() {
		VkDevice vkDevice = m_Device->GetVkDevice();

		// Create color attachment textures
		for (const auto& colorDesc : m_Desc.ColorAttachments) {
			if (colorDesc.ExistingTexture) {
				m_ColorAttachments.push_back(colorDesc.ExistingTexture);
			} else {
				TextureDesc texDesc;
				texDesc.Width = colorDesc.Width > 0 ? colorDesc.Width : m_Desc.Width;
				texDesc.Height = colorDesc.Height > 0 ? colorDesc.Height : m_Desc.Height;
				texDesc.Format = colorDesc.Format;
				texDesc.IsRenderTarget = true;
				texDesc.MipLevels = 1;
				texDesc.SampleCount = colorDesc.SampleCount;

				auto texture = CreateRef<VulkanRHITexture2D>(m_Device, texDesc);
				m_ColorAttachments.push_back(texture);
			}
		}

		// Create depth attachment texture
		if (m_Desc.HasDepth) {
			if (m_Desc.DepthAttachment.ExistingTexture) {
				m_DepthAttachment = m_Desc.DepthAttachment.ExistingTexture;
			} else {
				TextureDesc texDesc;
				texDesc.Width = m_Desc.DepthAttachment.Width > 0 ? m_Desc.DepthAttachment.Width : m_Desc.Width;
				texDesc.Height = m_Desc.DepthAttachment.Height > 0 ? m_Desc.DepthAttachment.Height : m_Desc.Height;
				texDesc.Format = m_Desc.DepthAttachment.Format;
				texDesc.IsRenderTarget = true;
				texDesc.MipLevels = 1;

				auto texture = CreateRef<VulkanRHITexture2D>(m_Device, texDesc);
				m_DepthAttachment = texture;
			}
		}

		// Create render pass
		std::vector<VkAttachmentDescription> attachments;
		std::vector<VkAttachmentReference> colorRefs;

		for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = VulkanTextureUtils::GetVkFormat(m_Desc.ColorAttachments[i].Format);
			colorAttachment.samples = static_cast<VkSampleCountFlagBits>(m_Desc.SampleCount);
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments.push_back(colorAttachment);

			VkAttachmentReference colorRef{};
			colorRef.attachment = static_cast<uint32_t>(i);
			colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorRefs.push_back(colorRef);
		}

		VkAttachmentReference depthRef{};
		if (m_Desc.HasDepth) {
			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = VulkanTextureUtils::GetVkFormat(m_Desc.DepthAttachment.Format);
			depthAttachment.samples = static_cast<VkSampleCountFlagBits>(m_Desc.SampleCount);
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(depthAttachment);

			depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
		subpass.pColorAttachments = colorRefs.data();
		subpass.pDepthStencilAttachment = m_Desc.HasDepth ? &depthRef : nullptr;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan render pass!");
			return;
		}

		// Collect image views for framebuffer creation
		std::vector<VkImageView> imageViews;
		for (auto& colorTex : m_ColorAttachments) {
			auto* vkTex = dynamic_cast<VulkanRHITexture2D*>(colorTex.get());
			if (vkTex) {
				imageViews.push_back(vkTex->GetVkImageView());
			}
		}
		if (m_DepthAttachment) {
			auto* vkTex = dynamic_cast<VulkanRHITexture2D*>(m_DepthAttachment.get());
			if (vkTex) {
				imageViews.push_back(vkTex->GetVkImageView());
			}
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = m_Desc.Width;
		framebufferInfo.height = m_Desc.Height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan framebuffer!");
		}
	}

	void VulkanRHIFramebuffer::DestroyFramebuffer() {
		VkDevice vkDevice = m_Device->GetVkDevice();

		if (m_Framebuffer != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(vkDevice, m_Framebuffer, nullptr);
			m_Framebuffer = VK_NULL_HANDLE;
		}
		if (m_RenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(vkDevice, m_RenderPass, nullptr);
			m_RenderPass = VK_NULL_HANDLE;
		}

		m_ColorAttachments.clear();
		m_DepthAttachment = nullptr;
	}

	void VulkanRHIFramebuffer::Bind() {
		// In Vulkan, framebuffer binding is done via vkCmdBeginRenderPass
	}

	void VulkanRHIFramebuffer::Unbind() {
		// No-op for Vulkan
	}

	void VulkanRHIFramebuffer::BindForRead() {
		// In Vulkan, reading is done through descriptor sets
	}

	void VulkanRHIFramebuffer::Resize(uint32_t width, uint32_t height) {
		if (width == m_Desc.Width && height == m_Desc.Height) return;

		m_Desc.Width = width;
		m_Desc.Height = height;

		for (auto& colorDesc : m_Desc.ColorAttachments) {
			colorDesc.Width = width;
			colorDesc.Height = height;
			colorDesc.ExistingTexture = nullptr;
		}
		if (m_Desc.HasDepth) {
			m_Desc.DepthAttachment.Width = width;
			m_Desc.DepthAttachment.Height = height;
			m_Desc.DepthAttachment.ExistingTexture = nullptr;
		}

		DestroyFramebuffer();
		CreateFramebuffer();
	}

	void VulkanRHIFramebuffer::Clear(const ClearValue& colorValue, float depth, uint8_t stencil) {
		// Clearing in Vulkan is done at render pass begin
	}

	void VulkanRHIFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value) {
		// TODO: Implement via vkCmdClearAttachments
		LNX_LOG_WARN("VulkanRHIFramebuffer::ClearAttachment - Not fully implemented");
	}

	void VulkanRHIFramebuffer::ClearDepth(float depth, uint8_t stencil) {
		// Clearing in Vulkan is done at render pass begin
	}

	Ref<RHITexture2D> VulkanRHIFramebuffer::GetColorAttachment(uint32_t index) const {
		if (index < m_ColorAttachments.size()) {
			return m_ColorAttachments[index];
		}
		return nullptr;
	}

	Ref<RHITexture2D> VulkanRHIFramebuffer::GetDepthAttachment() const {
		return m_DepthAttachment;
	}

	RHIHandle VulkanRHIFramebuffer::GetColorAttachmentID(uint32_t index) const {
		if (index < m_ColorAttachments.size() && m_ColorAttachments[index]) {
			return m_ColorAttachments[index]->GetNativeHandle();
		}
		return 0;
	}

	RHIHandle VulkanRHIFramebuffer::GetDepthAttachmentID() const {
		if (m_DepthAttachment) {
			return m_DepthAttachment->GetNativeHandle();
		}
		return 0;
	}

	int VulkanRHIFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) {
		// TODO: Implement via staging buffer readback
		LNX_LOG_WARN("VulkanRHIFramebuffer::ReadPixel - Not implemented");
		return -1;
	}

	void VulkanRHIFramebuffer::ReadPixels(uint32_t attachmentIndex, int x, int y,
										  uint32_t width, uint32_t height, void* buffer) {
		// TODO: Implement via staging buffer readback
		LNX_LOG_WARN("VulkanRHIFramebuffer::ReadPixels - Not implemented");
	}

	void VulkanRHIFramebuffer::BlitTo(RHIFramebuffer* dest, FilterMode filter) {
		// TODO: Implement via vkCmdBlitImage
		LNX_LOG_WARN("VulkanRHIFramebuffer::BlitTo - Not implemented");
	}

	void VulkanRHIFramebuffer::BlitToScreen(uint32_t screenWidth, uint32_t screenHeight, FilterMode filter) {
		// TODO: Implement via swapchain blit
		LNX_LOG_WARN("VulkanRHIFramebuffer::BlitToScreen - Not implemented");
	}

	void VulkanRHIFramebuffer::OnDebugNameChanged() {
		// TODO: Set VK_EXT_debug_utils object name
	}

} // namespace RHI
} // namespace Lunex
