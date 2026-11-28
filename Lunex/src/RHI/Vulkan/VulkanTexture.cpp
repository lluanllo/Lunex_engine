#include "stpch.h"
#include "VulkanTexture.h"
#include "VulkanState.h"

#include "Log/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace Lunex::RHI::Vk {

	BindingState& GetBindingState() {
		static BindingState s_State;
		return s_State;
	}

	// ============================================================================
	// FORMATS
	// ============================================================================

	VkFormat ToVkFormat(TextureFormat f) {
		switch (f) {
			case TextureFormat::R8: return VK_FORMAT_R8_UNORM;
			case TextureFormat::RG8: return VK_FORMAT_R8G8_UNORM;
			case TextureFormat::RGB8: return VK_FORMAT_R8G8B8A8_UNORM;     // expanded on upload
			case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
			case TextureFormat::SRGB8: return VK_FORMAT_R8G8B8A8_SRGB;
			case TextureFormat::SRGBA8: return VK_FORMAT_R8G8B8A8_SRGB;
			case TextureFormat::R16F: return VK_FORMAT_R16_SFLOAT;
			case TextureFormat::RG16F: return VK_FORMAT_R16G16_SFLOAT;
			case TextureFormat::RGB16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
			case TextureFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
			case TextureFormat::R32F: return VK_FORMAT_R32_SFLOAT;
			case TextureFormat::RG32F: return VK_FORMAT_R32G32_SFLOAT;
			case TextureFormat::RGB32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case TextureFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case TextureFormat::R32I: return VK_FORMAT_R32_SINT;
			case TextureFormat::RG32I: return VK_FORMAT_R32G32_SINT;
			case TextureFormat::RGBA32I: return VK_FORMAT_R32G32B32A32_SINT;
			case TextureFormat::R32UI: return VK_FORMAT_R32_UINT;
			case TextureFormat::Depth16: return VK_FORMAT_D16_UNORM;
			case TextureFormat::Depth24: return VK_FORMAT_X8_D24_UNORM_PACK32;
			case TextureFormat::Depth32F: return VK_FORMAT_D32_SFLOAT;
			case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
			case TextureFormat::Depth32FStencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
			case TextureFormat::BC1: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			case TextureFormat::BC1_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
			case TextureFormat::BC3: return VK_FORMAT_BC3_UNORM_BLOCK;
			case TextureFormat::BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
			case TextureFormat::BC4: return VK_FORMAT_BC4_UNORM_BLOCK;
			case TextureFormat::BC5: return VK_FORMAT_BC5_UNORM_BLOCK;
			case TextureFormat::BC6H: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case TextureFormat::BC7: return VK_FORMAT_BC7_UNORM_BLOCK;
			case TextureFormat::BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
			default: return VK_FORMAT_R8G8B8A8_UNORM;
		}
	}

	uint32_t BytesPerPixel(TextureFormat f) {
		switch (f) {
			case TextureFormat::R8: return 1;
			case TextureFormat::RG8: case TextureFormat::R16F: case TextureFormat::Depth16: return 2;
			case TextureFormat::RGB8: case TextureFormat::SRGB8: return 3;
			case TextureFormat::RGB16F: return 6;
			case TextureFormat::RG16F: case TextureFormat::R32F: case TextureFormat::R32I: case TextureFormat::R32UI:
			case TextureFormat::RGBA8: case TextureFormat::SRGBA8: case TextureFormat::Depth24: case TextureFormat::Depth32F:
			case TextureFormat::Depth24Stencil8: return 4;
			case TextureFormat::RGBA16F: case TextureFormat::RG32F: case TextureFormat::RG32I: return 8;
			case TextureFormat::RGB32F: return 12;
			case TextureFormat::RGBA32F: case TextureFormat::RGBA32I: return 16;
			default: return 4;
		}
	}

	VkImageAspectFlags AspectOf(TextureFormat f) {
		if (f == TextureFormat::Depth24Stencil8 || f == TextureFormat::Depth32FStencil8) return VK_IMAGE_ASPECT_DEPTH_BIT;   // sampled as depth
		if (IsDepthFormat(f)) return VK_IMAGE_ASPECT_DEPTH_BIT;
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}

	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
	                     uint32_t mipLevels, uint32_t layers) {
		VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		b.oldLayout = from;
		b.newLayout = to;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = image;
		b.subresourceRange = { aspect, 0, mipLevels, 0, layers };
		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	// ============================================================================
	// TEXTURE 2D
	// ============================================================================

	VulkanTexture2D::VulkanTexture2D(const TextureDesc& desc, const void* data, uint64_t size) {
		m_Desc = desc;
		if (m_Desc.GenerateMipmaps && m_Desc.MipLevels <= 1)
			m_Desc.MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_Desc.Width, m_Desc.Height)))) + 1;
		m_Desc.MipLevels = std::max(1u, m_Desc.MipLevels);
		Create();
		if (data && size) {
			Upload(data, size, 0, 0, m_Desc.Width, m_Desc.Height, 0);
			if (m_Desc.MipLevels > 1) GenerateMipmaps();
		}
	}

	VulkanTexture2D::~VulkanTexture2D() { Destroy(); }

	void VulkanTexture2D::Create() {
		auto& ctx = VulkanContext::Get();
		m_Format = ToVkFormat(m_Desc.Format);
		const bool depth = RHI::IsDepthFormat(m_Desc.Format);

		VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = m_Format;
		info.extent = { std::max(1u, m_Desc.Width), std::max(1u, m_Desc.Height), 1 };
		info.mipLevels = m_Desc.MipLevels;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (m_Desc.IsRenderTarget) info.usage |= depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (m_Desc.IsStorage) info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (vkCreateImage(ctx.GetDevice(), &info, nullptr, &m_Image) != VK_SUCCESS) {
			LNX_LOG_ERROR("Vulkan: image creation failed ({0}x{1})", m_Desc.Width, m_Desc.Height);
			return;
		}
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(ctx.GetDevice(), m_Image, &req);
		m_Memory = ctx.Allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkBindImageMemory(ctx.GetDevice(), m_Image, m_Memory.Memory, m_Memory.Offset);

		VkImageViewCreateInfo view{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view.image = m_Image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = m_Format;
		view.subresourceRange = { AspectOf(m_Desc.Format), 0, m_Desc.MipLevels, 0, 1 };
		// Single channel textures read as grey in ImGui / samplers expecting .rgb (GL swizzle behaviour)
		if (m_Desc.Format == TextureFormat::R8) view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE };
		vkCreateImageView(ctx.GetDevice(), &view, nullptr, &m_View);

		// Everything starts readable (sampling an untouched render target = zeros, like GL)
		const VkImageLayout ready = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const VkImage image = m_Image;
		const VkImageAspectFlags aspect = AspectOf(m_Desc.Format);
		const bool clearable = !RHI::IsCompressedFormat(m_Desc.Format);
		ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
			TransitionImage(cmd, image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			if (clearable) {
				VkImageSubresourceRange range{ aspect, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
				if (depth) { VkClearDepthStencilValue v{ 1.0f, 0 }; vkCmdClearDepthStencilImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range); }
				else { VkClearColorValue v{}; vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range); }
			}
			TransitionImage(cmd, image, aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ready);
		});
		m_Layout = ready;
	}

	void VulkanTexture2D::Destroy() {
		if (!VulkanContext::IsActive()) return;
		auto& ctx = VulkanContext::Get();
		if (m_ImGuiID) { ctx.UnregisterImGuiTexture(m_ImGuiID); m_ImGuiID = 0; }
		const VkImage image = m_Image;
		const VkImageView view = m_View;
		const Allocation memory = m_Memory;
		const VkDevice device = ctx.GetDevice();
		ctx.DeferDelete([device, image, view, memory] {
			if (view) vkDestroyImageView(device, view, nullptr);
			if (image) vkDestroyImage(device, image, nullptr);
			VulkanContext::Get().Free(memory);
		});
		m_Image = VK_NULL_HANDLE;
		m_View = VK_NULL_HANDLE;
		m_Memory = {};
	}

	void VulkanTexture2D::Upload(const void* data, uint64_t size, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t mip) {
		if (!m_Image || !data) return;
		auto& ctx = VulkanContext::Get();

		// RGB data goes into RGBA images
		std::vector<uint8_t> expanded;
		const uint32_t srcBpp = BytesPerPixel(m_Desc.Format);
		if (!RHI::IsCompressedFormat(m_Desc.Format) && (m_Desc.Format == TextureFormat::RGB8 || m_Desc.Format == TextureFormat::SRGB8)) {
			const uint64_t pixels = static_cast<uint64_t>(w) * h;
			if (size >= pixels * 4) { /* already RGBA */ }
			else {
				expanded.resize(pixels * 4);
				const uint8_t* src = static_cast<const uint8_t*>(data);
				for (uint64_t i = 0; i < pixels; i++) {
					expanded[i * 4 + 0] = src[i * srcBpp + 0];
					expanded[i * 4 + 1] = src[i * srcBpp + 1];
					expanded[i * 4 + 2] = src[i * srcBpp + 2];
					expanded[i * 4 + 3] = 255;
				}
				data = expanded.data();
				size = expanded.size();
			}
		}

		VkBufferCreateInfo binfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		binfo.size = size;
		binfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VkBuffer staging;
		vkCreateBuffer(ctx.GetDevice(), &binfo, nullptr, &staging);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(ctx.GetDevice(), staging, &req);
		Allocation mem = ctx.Allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkBindBufferMemory(ctx.GetDevice(), staging, mem.Memory, mem.Offset);
		std::memcpy(mem.Mapped, data, size);

		const VkImage image = m_Image;
		const VkImageAspectFlags aspect = AspectOf(m_Desc.Format);
		const VkImageLayout prev = m_Layout;
		ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
			TransitionImage(cmd, image, aspect, prev, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkBufferImageCopy copy{};
			copy.imageSubresource = { aspect, mip, 0, 1 };
			copy.imageOffset = { static_cast<int32_t>(x), static_cast<int32_t>(y), 0 };
			copy.imageExtent = { w, h, 1 };
			vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
			TransitionImage(cmd, image, aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, prev == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : prev);
		});
		if (m_Layout == VK_IMAGE_LAYOUT_UNDEFINED) m_Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkDestroyBuffer(ctx.GetDevice(), staging, nullptr);
		ctx.Free(mem);
	}

	void VulkanTexture2D::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		const uint32_t mipW = std::max(1u, m_Desc.Width >> region.MipLevel), mipH = std::max(1u, m_Desc.Height >> region.MipLevel);
		const uint32_t w = region.Width ? region.Width : mipW;
		const uint32_t h = region.Height ? region.Height : mipH;
		Upload(data, size, region.X, region.Y, w, h, region.MipLevel);
		if (region.MipLevel == 0 && w == m_Desc.Width && h == m_Desc.Height && m_Desc.MipLevels > 1) GenerateMipmaps();
	}

	void VulkanTexture2D::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		if (!m_Image || !data) return;
		auto& ctx = VulkanContext::Get();
		const uint32_t w = region.Width ? region.Width : m_Desc.Width;
		const uint32_t h = region.Height ? region.Height : m_Desc.Height;
		VkBufferCreateInfo binfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		binfo.size = std::max<uint64_t>(size, 4);
		binfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VkBuffer readback;
		vkCreateBuffer(ctx.GetDevice(), &binfo, nullptr, &readback);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(ctx.GetDevice(), readback, &req);
		Allocation mem = ctx.Allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		vkBindBufferMemory(ctx.GetDevice(), readback, mem.Memory, mem.Offset);
		const VkImage image = m_Image;
		const VkImageAspectFlags aspect = AspectOf(m_Desc.Format);
		const VkImageLayout layout = m_Layout;
		ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
			TransitionImage(cmd, image, aspect, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			VkBufferImageCopy copy{};
			copy.imageSubresource = { aspect, region.MipLevel, 0, 1 };
			copy.imageOffset = { region.X, region.Y, 0 };
			copy.imageExtent = { w, h, 1 };
			vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
			TransitionImage(cmd, image, aspect, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);
		});
		std::memcpy(data, mem.Mapped, size);
		vkDestroyBuffer(ctx.GetDevice(), readback, nullptr);
		ctx.Free(mem);
	}

	void VulkanTexture2D::GenerateMipmaps() {
		if (!m_Image || m_Desc.MipLevels <= 1 || RHI::IsDepthFormat(m_Desc.Format) || RHI::IsCompressedFormat(m_Desc.Format)) return;
		auto& ctx = VulkanContext::Get();
		const VkImage image = m_Image;
		const uint32_t mips = m_Desc.MipLevels;
		int32_t w = static_cast<int32_t>(m_Desc.Width), h = static_cast<int32_t>(m_Desc.Height);
		const VkImageLayout layout = m_Layout;
		ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
			TransitionImage(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			for (uint32_t i = 1; i < mips; i++) {
				VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
				b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				b.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; b.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
				b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.image = image;
				b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 };
				VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
				dep.imageMemoryBarrierCount = 1;
				dep.pImageMemoryBarriers = &b;
				vkCmdPipelineBarrier2(cmd, &dep);
				VkImageBlit blit{};
				blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
				blit.srcOffsets[1] = { w, h, 1 };
				blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
				blit.dstOffsets[1] = { std::max(1, w / 2), std::max(1, h / 2), 1 };
				vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
				w = std::max(1, w / 2);
				h = std::max(1, h / 2);
			}
			// All but the last level are TRANSFER_SRC now; the last one TRANSFER_DST
			VkImageMemoryBarrier2 b[2]{};
			for (auto& x : b) {
				x.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				x.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; x.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				x.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; x.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
				x.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				x.srcQueueFamilyIndex = x.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				x.image = image;
			}
			b[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			b[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mips - 1, 0, 1 };
			b[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mips - 1, 1, 0, 1 };
			VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dep.imageMemoryBarrierCount = 2;
			dep.pImageMemoryBarriers = b;
			vkCmdPipelineBarrier2(cmd, &dep);
		});
		m_Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	void VulkanTexture2D::Transition(VkCommandBuffer cmd, VkImageLayout to) const {
		if (m_Layout == to || !m_Image) return;
		TransitionImage(cmd, m_Image, AspectOf(m_Desc.Format), m_Layout, to);
		m_Layout = to;
	}

	VkSampler VulkanTexture2D::GetDefaultSampler() const {
		auto& ctx = VulkanContext::Get();
		const bool integer = m_Format == VK_FORMAT_R32_SINT || m_Format == VK_FORMAT_R32_UINT || m_Format == VK_FORMAT_R32G32_SINT ||
			m_Format == VK_FORMAT_R32G32_UINT || m_Format == VK_FORMAT_R32G32B32A32_SINT || m_Format == VK_FORMAT_R32G32B32A32_UINT;
		if (RHI::IsDepthFormat(m_Desc.Format) || integer) return ctx.GetSampler(VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		if (m_Desc.IsRenderTarget) return ctx.GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		return ctx.GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, false, 8.0f);
	}

	void VulkanTexture2D::Bind(uint32_t slot) const { if (slot < kMaxTextureUnits) GetBindingState().Textures[slot] = this; }
	void VulkanTexture2D::Unbind(uint32_t slot) const { if (slot < kMaxTextureUnits && GetBindingState().Textures[slot] == this) GetBindingState().Textures[slot] = nullptr; }
	void VulkanTexture2D::BindAsImage(uint32_t slot, BufferAccess, uint32_t mip) const {
		if (slot >= kMaxTextureUnits) return;
		GetBindingState().StorageImages[slot] = this;
		GetBindingState().StorageImageMips[slot] = mip;
	}

	void VulkanTexture2D::Resize(uint32_t width, uint32_t height) {
		if (width == m_Desc.Width && height == m_Desc.Height) return;
		const uint32_t imguiID = m_ImGuiID;
		m_ImGuiID = 0;
		Destroy();
		m_Desc.Width = width;
		m_Desc.Height = height;
		Create();
		if (imguiID) {
			m_ImGuiID = imguiID;
			VulkanContext::Get().UpdateImGuiTexture(imguiID, m_View, GetDefaultSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	int VulkanTexture2D::ReadPixel(int x, int y) const {
		int value = -1;
		TextureRegion r;
		r.X = x; r.Y = y; r.Width = 1; r.Height = 1;
		GetData(&value, sizeof(value), r);
		return value;
	}

	void VulkanTexture2D::Clear(const ClearValue& value) {
		if (!m_Image) return;
		auto& ctx = VulkanContext::Get();
		const VkImage image = m_Image;
		const bool depth = RHI::IsDepthFormat(m_Desc.Format);
		const VkImageAspectFlags aspect = AspectOf(m_Desc.Format);
		const VkImageLayout layout = m_Layout;
		ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
			TransitionImage(cmd, image, aspect, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkImageSubresourceRange range{ aspect, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
			if (depth) { VkClearDepthStencilValue v{ value.DepthStencil.Depth, value.DepthStencil.Stencil }; vkCmdClearDepthStencilImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range); }
			else { VkClearColorValue v{ { value.Color[0], value.Color[1], value.Color[2], value.Color[3] } }; vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range); }
			TransitionImage(cmd, image, aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);
		});
	}

	uint32_t VulkanTexture2D::GetRendererID() const {
		// Lazily registered: only textures shown by ImGui get a descriptor set
		if (!m_ImGuiID && m_View && !RHI::IsDepthFormat(m_Desc.Format))
			m_ImGuiID = VulkanContext::Get().RegisterImGuiTexture(m_View, GetDefaultSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return m_ImGuiID;
	}

}
