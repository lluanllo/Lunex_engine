/**
 * @file VulkanRHITexture.cpp
 * @brief Vulkan implementation of RHI texture types
 */

#include "stpch.h"
#include "VulkanRHITexture.h"
#include "VulkanRHIDevice.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// VULKAN TEXTURE UTILITIES
	// ============================================================================

	namespace VulkanTextureUtils {

		VkFormat GetVkFormat(TextureFormat format) {
			switch (format) {
				// 8-bit
				case TextureFormat::R8:            return VK_FORMAT_R8_UNORM;
				case TextureFormat::RG8:           return VK_FORMAT_R8G8_UNORM;
				case TextureFormat::RGB8:          return VK_FORMAT_R8G8B8_UNORM;
				case TextureFormat::RGBA8:         return VK_FORMAT_R8G8B8A8_UNORM;
				case TextureFormat::SRGB8:         return VK_FORMAT_R8G8B8_SRGB;
				case TextureFormat::SRGBA8:        return VK_FORMAT_R8G8B8A8_SRGB;

				// 16-bit float
				case TextureFormat::R16F:          return VK_FORMAT_R16_SFLOAT;
				case TextureFormat::RG16F:         return VK_FORMAT_R16G16_SFLOAT;
				case TextureFormat::RGB16F:        return VK_FORMAT_R16G16B16_SFLOAT;
				case TextureFormat::RGBA16F:       return VK_FORMAT_R16G16B16A16_SFLOAT;

				// 32-bit float
				case TextureFormat::R32F:          return VK_FORMAT_R32_SFLOAT;
				case TextureFormat::RG32F:         return VK_FORMAT_R32G32_SFLOAT;
				case TextureFormat::RGB32F:        return VK_FORMAT_R32G32B32_SFLOAT;
				case TextureFormat::RGBA32F:       return VK_FORMAT_R32G32B32A32_SFLOAT;

				// Integer
				case TextureFormat::R32I:          return VK_FORMAT_R32_SINT;
				case TextureFormat::RG32I:         return VK_FORMAT_R32G32_SINT;
				case TextureFormat::RGBA32I:       return VK_FORMAT_R32G32B32A32_SINT;
				case TextureFormat::R32UI:         return VK_FORMAT_R32_UINT;

				// Depth/Stencil
				case TextureFormat::Depth16:       return VK_FORMAT_D16_UNORM;
				case TextureFormat::Depth24:       return VK_FORMAT_X8_D24_UNORM_PACK32;
				case TextureFormat::Depth32F:      return VK_FORMAT_D32_SFLOAT;
				case TextureFormat::Depth24Stencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
				case TextureFormat::Depth32FStencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;

				// Compressed
				case TextureFormat::BC1:           return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
				case TextureFormat::BC1_SRGB:      return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
				case TextureFormat::BC3:           return VK_FORMAT_BC3_UNORM_BLOCK;
				case TextureFormat::BC3_SRGB:      return VK_FORMAT_BC3_SRGB_BLOCK;
				case TextureFormat::BC4:           return VK_FORMAT_BC4_UNORM_BLOCK;
				case TextureFormat::BC5:           return VK_FORMAT_BC5_UNORM_BLOCK;
				case TextureFormat::BC7:           return VK_FORMAT_BC7_UNORM_BLOCK;
				case TextureFormat::BC7_SRGB:      return VK_FORMAT_BC7_SRGB_BLOCK;

				default: return VK_FORMAT_R8G8B8A8_UNORM;
			}
		}

		VkImageAspectFlags GetAspectFlags(TextureFormat format) {
			if (IsDepthFormat(format)) {
				VkImageAspectFlags flags = VK_IMAGE_ASPECT_DEPTH_BIT;
				if (format == TextureFormat::Depth24Stencil8 || format == TextureFormat::Depth32FStencil8) {
					flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
				return flags;
			}
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}

		uint32_t CalculateMipCount(uint32_t width, uint32_t height) {
			return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

	} // namespace VulkanTextureUtils

	// ============================================================================
	// VULKAN RHI TEXTURE 2D
	// ============================================================================

	VulkanRHITexture2D::VulkanRHITexture2D(VulkanRHIDevice* device, const TextureDesc& desc, const void* initialData)
		: m_Device(device)
	{
		m_Desc = desc;
		m_VkFormat = VulkanTextureUtils::GetVkFormat(desc.Format);
		CreateImage(initialData);
	}

	VulkanRHITexture2D::~VulkanRHITexture2D() {
		DestroyImage();
	}

	void VulkanRHITexture2D::CreateImage(const void* data) {
		VkDevice vkDevice = m_Device->GetVkDevice();

		// Create image
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_Desc.Width;
		imageInfo.extent.height = m_Desc.Height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = m_Desc.MipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_VkFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.samples = static_cast<VkSampleCountFlagBits>(m_Desc.SampleCount);
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (m_Desc.IsRenderTarget) {
			if (IsDepthFormat(m_Desc.Format))
				imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (m_Desc.IsStorage) {
			imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		if (m_Desc.GenerateMipmaps) {
			imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		if (vkCreateImage(vkDevice, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan image!");
			return;
		}

		// Allocate memory
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(vkDevice, m_Image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = m_Device->FindMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to allocate Vulkan image memory!");
			vkDestroyImage(vkDevice, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
			return;
		}

		vkBindImageMemory(vkDevice, m_Image, m_Memory, 0);
		m_Device->TrackAllocation(memRequirements.size);

		// Create image view
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_VkFormat;
		viewInfo.subresourceRange.aspectMask = VulkanTextureUtils::GetAspectFlags(m_Desc.Format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = m_Desc.MipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan image view!");
		}

		// Upload initial data if provided
		if (data) {
			// TODO: Implement staging buffer upload for initial data
			LNX_LOG_WARN("VulkanRHITexture2D: Initial data upload not fully implemented");
		}

		m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	void VulkanRHITexture2D::DestroyImage() {
		VkDevice vkDevice = m_Device->GetVkDevice();

		if (m_ImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(vkDevice, m_ImageView, nullptr);
			m_ImageView = VK_NULL_HANDLE;
		}
		if (m_Image != VK_NULL_HANDLE) {
			vkDestroyImage(vkDevice, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
		}
		if (m_Memory != VK_NULL_HANDLE) {
			vkFreeMemory(vkDevice, m_Memory, nullptr);
			m_Memory = VK_NULL_HANDLE;
		}
	}

	void VulkanRHITexture2D::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
		VulkanRHIContext* context = m_Device->GetVulkanContext();
		VkCommandBuffer cmdBuf = context->BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_Image;
		barrier.subresourceRange.aspectMask = VulkanTextureUtils::GetAspectFlags(m_Desc.Format);
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = m_Desc.MipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		vkCmdPipelineBarrier(cmdBuf, sourceStage, destinationStage,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		context->EndSingleTimeCommands(cmdBuf);
		m_CurrentLayout = newLayout;
	}

	void VulkanRHITexture2D::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		// TODO: Implement via staging buffer
		LNX_LOG_WARN("VulkanRHITexture2D::SetData - Not fully implemented");
	}

	void VulkanRHITexture2D::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		// TODO: Implement via staging buffer readback
		LNX_LOG_WARN("VulkanRHITexture2D::GetData - Not fully implemented");
	}

	void VulkanRHITexture2D::GenerateMipmaps() {
		// TODO: Implement mipmap generation via vkCmdBlitImage
		LNX_LOG_WARN("VulkanRHITexture2D::GenerateMipmaps - Not fully implemented");
	}

	void VulkanRHITexture2D::Bind(uint32_t slot) const {
		// In Vulkan, textures are bound through descriptor sets
	}

	void VulkanRHITexture2D::Unbind(uint32_t slot) const {
		// No-op for Vulkan
	}

	void VulkanRHITexture2D::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {
		// In Vulkan, this is done through descriptor sets with VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	}

	void VulkanRHITexture2D::Resize(uint32_t width, uint32_t height) {
		if (width == m_Desc.Width && height == m_Desc.Height) return;

		DestroyImage();
		m_Desc.Width = width;
		m_Desc.Height = height;
		CreateImage(nullptr);
	}

	int VulkanRHITexture2D::ReadPixel(int x, int y) const {
		// TODO: Implement via staging buffer readback
		LNX_LOG_WARN("VulkanRHITexture2D::ReadPixel - Not implemented");
		return -1;
	}

	void VulkanRHITexture2D::Clear(const ClearValue& value) {
		// TODO: Implement via vkCmdClearColorImage or render pass
		LNX_LOG_WARN("VulkanRHITexture2D::Clear - Not implemented");
	}

	void VulkanRHITexture2D::OnDebugNameChanged() {
		// TODO: Set VK_EXT_debug_utils object name
	}

	// ============================================================================
	// VULKAN RHI TEXTURE CUBE
	// ============================================================================

	VulkanRHITextureCube::VulkanRHITextureCube(VulkanRHIDevice* device, const TextureDesc& desc)
		: m_Device(device)
	{
		m_Desc = desc;
		m_VkFormat = VulkanTextureUtils::GetVkFormat(desc.Format);

		VkDevice vkDevice = m_Device->GetVkDevice();

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = desc.Width;
		imageInfo.extent.height = desc.Height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = desc.MipLevels;
		imageInfo.arrayLayers = 6;
		imageInfo.format = m_VkFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		if (vkCreateImage(vkDevice, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan cubemap image!");
			return;
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(vkDevice, m_Image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = m_Device->FindMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to allocate Vulkan cubemap memory!");
			vkDestroyImage(vkDevice, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
			return;
		}

		vkBindImageMemory(vkDevice, m_Image, m_Memory, 0);
		m_Device->TrackAllocation(memRequirements.size);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.format = m_VkFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = desc.MipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6;

		if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan cubemap image view!");
		}
	}

	VulkanRHITextureCube::~VulkanRHITextureCube() {
		VkDevice vkDevice = m_Device->GetVkDevice();
		if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(vkDevice, m_ImageView, nullptr);
		if (m_Image != VK_NULL_HANDLE) vkDestroyImage(vkDevice, m_Image, nullptr);
		if (m_Memory != VK_NULL_HANDLE) vkFreeMemory(vkDevice, m_Memory, nullptr);
	}

	void VulkanRHITextureCube::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		LNX_LOG_WARN("VulkanRHITextureCube::SetData - Not fully implemented");
	}

	void VulkanRHITextureCube::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		LNX_LOG_WARN("VulkanRHITextureCube::GetData - Not fully implemented");
	}

	void VulkanRHITextureCube::GenerateMipmaps() {
		LNX_LOG_WARN("VulkanRHITextureCube::GenerateMipmaps - Not fully implemented");
	}

	void VulkanRHITextureCube::SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel) {
		LNX_LOG_WARN("VulkanRHITextureCube::SetFaceData - Not fully implemented");
	}

	void VulkanRHITextureCube::Bind(uint32_t slot) const {}
	void VulkanRHITextureCube::Unbind(uint32_t slot) const {}
	void VulkanRHITextureCube::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {}
	void VulkanRHITextureCube::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI SAMPLER
	// ============================================================================

	static VkFilter GetVkFilter(FilterMode mode) {
		switch (mode) {
			case FilterMode::Nearest:
			case FilterMode::NearestMipmapNearest:
			case FilterMode::NearestMipmapLinear:
				return VK_FILTER_NEAREST;
			default:
				return VK_FILTER_LINEAR;
		}
	}

	static VkSamplerMipmapMode GetVkMipmapMode(FilterMode mode) {
		switch (mode) {
			case FilterMode::NearestMipmapNearest:
			case FilterMode::LinearMipmapNearest:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			default:
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
	}

	static VkSamplerAddressMode GetVkWrapMode(WrapMode mode) {
		switch (mode) {
			case WrapMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case WrapMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case WrapMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case WrapMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	static VkCompareOp GetVkCompareOp(CompareFunc func) {
		switch (func) {
			case CompareFunc::Never:        return VK_COMPARE_OP_NEVER;
			case CompareFunc::Less:         return VK_COMPARE_OP_LESS;
			case CompareFunc::Equal:        return VK_COMPARE_OP_EQUAL;
			case CompareFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
			case CompareFunc::Greater:      return VK_COMPARE_OP_GREATER;
			case CompareFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
			case CompareFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
			case CompareFunc::Always:       return VK_COMPARE_OP_ALWAYS;
			default: return VK_COMPARE_OP_NEVER;
		}
	}

	VulkanRHISampler::VulkanRHISampler(VulkanRHIDevice* device, const SamplerState& state)
		: m_Device(device)
	{
		m_State = state;

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = GetVkFilter(state.MagFilter);
		samplerInfo.minFilter = GetVkFilter(state.MinFilter);
		samplerInfo.mipmapMode = GetVkMipmapMode(state.MinFilter);
		samplerInfo.addressModeU = GetVkWrapMode(state.WrapU);
		samplerInfo.addressModeV = GetVkWrapMode(state.WrapV);
		samplerInfo.addressModeW = GetVkWrapMode(state.WrapW);
		samplerInfo.mipLodBias = state.MipLODBias;
		samplerInfo.anisotropyEnable = state.MaxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = state.MaxAnisotropy;
		samplerInfo.compareEnable = state.ComparisonFunc != CompareFunc::Never ? VK_TRUE : VK_FALSE;
		samplerInfo.compareOp = GetVkCompareOp(state.ComparisonFunc);
		samplerInfo.minLod = state.MinLOD;
		samplerInfo.maxLod = state.MaxLOD;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		if (vkCreateSampler(m_Device->GetVkDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan sampler!");
		}
	}

	VulkanRHISampler::~VulkanRHISampler() {
		if (m_Sampler != VK_NULL_HANDLE) {
			vkDestroySampler(m_Device->GetVkDevice(), m_Sampler, nullptr);
			m_Sampler = VK_NULL_HANDLE;
		}
	}

	void VulkanRHISampler::Bind(uint32_t slot) const {}
	void VulkanRHISampler::Unbind(uint32_t slot) const {}
	void VulkanRHISampler::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI TEXTURE 2D ARRAY
	// ============================================================================

	VulkanRHITexture2DArray::VulkanRHITexture2DArray(VulkanRHIDevice* device, const TextureDesc& desc)
		: m_Device(device)
	{
		m_Desc = desc;
		m_VkFormat = VulkanTextureUtils::GetVkFormat(desc.Format);

		VkDevice vkDevice = m_Device->GetVkDevice();

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = desc.Width;
		imageInfo.extent.height = desc.Height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = desc.MipLevels;
		imageInfo.arrayLayers = desc.ArrayLayers;
		imageInfo.format = m_VkFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (IsDepthFormat(desc.Format))
			imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (vkCreateImage(vkDevice, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan texture 2D array!");
			return;
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(vkDevice, m_Image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = m_Device->FindMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to allocate Vulkan texture array memory!");
			vkDestroyImage(vkDevice, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
			return;
		}

		vkBindImageMemory(vkDevice, m_Image, m_Memory, 0);
		m_Device->TrackAllocation(memRequirements.size);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		viewInfo.format = m_VkFormat;
		viewInfo.subresourceRange.aspectMask = VulkanTextureUtils::GetAspectFlags(desc.Format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = desc.MipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = desc.ArrayLayers;

		if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan texture array image view!");
		}
	}

	VulkanRHITexture2DArray::~VulkanRHITexture2DArray() {
		VkDevice vkDevice = m_Device->GetVkDevice();
		if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(vkDevice, m_ImageView, nullptr);
		if (m_Image != VK_NULL_HANDLE) vkDestroyImage(vkDevice, m_Image, nullptr);
		if (m_Memory != VK_NULL_HANDLE) vkFreeMemory(vkDevice, m_Memory, nullptr);
	}

	void VulkanRHITexture2DArray::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		LNX_LOG_WARN("VulkanRHITexture2DArray::SetData - Not fully implemented");
	}

	void VulkanRHITexture2DArray::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		LNX_LOG_WARN("VulkanRHITexture2DArray::GetData - Not fully implemented");
	}

	void VulkanRHITexture2DArray::GenerateMipmaps() {
		LNX_LOG_WARN("VulkanRHITexture2DArray::GenerateMipmaps - Not fully implemented");
	}

	void VulkanRHITexture2DArray::SetLayerData(uint32_t layer, const void* data, uint64_t size, uint32_t mipLevel) {
		LNX_LOG_WARN("VulkanRHITexture2DArray::SetLayerData - Not fully implemented");
	}

	void VulkanRHITexture2DArray::Bind(uint32_t slot) const {}
	void VulkanRHITexture2DArray::Unbind(uint32_t slot) const {}
	void VulkanRHITexture2DArray::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {}
	void VulkanRHITexture2DArray::OnDebugNameChanged() {}

} // namespace RHI
} // namespace Lunex
