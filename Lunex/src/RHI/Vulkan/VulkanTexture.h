#pragma once

/**
 * @file VulkanTexture.h
 * @brief Vulkan 2D textures (sampled images, render targets) for the RHI.
 *
 * Layout tracking: images live in SHADER_READ_ONLY_OPTIMAL between uses; render targets
 * move to attachment layouts while rendered to. GetRendererID() is the ImGui texture ID
 * (registered lazily), so ImGui::Image works exactly like with GL texture names.
 */

#include "RHI/RHITexture.h"
#include "VulkanContext.h"

namespace Lunex::RHI::Vk {

	VkFormat ToVkFormat(TextureFormat format);
	uint32_t BytesPerPixel(TextureFormat format);
	VkImageAspectFlags AspectOf(TextureFormat format);

	class VulkanTexture2D : public RHITexture2D {
	public:
		explicit VulkanTexture2D(const TextureDesc& desc, const void* data = nullptr, uint64_t size = 0);
		~VulkanTexture2D() override;

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Image); }
		bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

		// RHITexture
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;

		// RHITexture2D
		void Resize(uint32_t width, uint32_t height) override;
		int ReadPixel(int x, int y) const override;
		void Clear(const ClearValue& value) override;
		uint32_t GetRendererID() const override;

		// Vulkan
		VkImage GetImage() const { return m_Image; }
		VkImageView GetView() const { return m_View; }
		VkFormat GetVkFormat() const { return m_Format; }
		VkImageLayout GetLayout() const { return m_Layout; }
		// Records a layout transition in `cmd` (no-op when already there)
		void Transition(VkCommandBuffer cmd, VkImageLayout to) const;
		VkSampler GetDefaultSampler() const;

	private:
		void Create();
		void Destroy();
		void Upload(const void* data, uint64_t size, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t mip);

		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_View = VK_NULL_HANDLE;
		VkFormat m_Format = VK_FORMAT_UNDEFINED;
		Allocation m_Memory;
		mutable VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		mutable uint32_t m_ImGuiID = 0;
	};

	// Image layout barrier covering every mip / layer
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
	                     uint32_t mipLevels = VK_REMAINING_MIP_LEVELS, uint32_t layers = VK_REMAINING_ARRAY_LAYERS);

}
