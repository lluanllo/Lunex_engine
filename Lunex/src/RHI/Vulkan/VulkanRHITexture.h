/**
 * @file VulkanRHITexture.h
 * @brief Vulkan implementation of RHI texture types
 *
 * Wraps VkImage + VkImageView + VkDeviceMemory for 2D, cube, and array textures.
 */

#pragma once

#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHIDevice;

	// ============================================================================
	// VULKAN TEXTURE UTILITIES
	// ============================================================================

	namespace VulkanTextureUtils {

		VkFormat GetVkFormat(TextureFormat format);
		VkImageAspectFlags GetAspectFlags(TextureFormat format);
		uint32_t CalculateMipCount(uint32_t width, uint32_t height);

	} // namespace VulkanTextureUtils

	// ============================================================================
	// VULKAN RHI TEXTURE 2D
	// ============================================================================

	class VulkanRHITexture2D : public RHITexture2D {
	public:
		VulkanRHITexture2D(VulkanRHIDevice* device, const TextureDesc& desc, const void* initialData = nullptr);
		virtual ~VulkanRHITexture2D();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Image); }
		bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;

		// Binding
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;

		// Texture2D specific
		void Resize(uint32_t width, uint32_t height) override;
		int ReadPixel(int x, int y) const override;
		void Clear(const ClearValue& value) override;
		uint32_t GetRendererID() const override { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m_ImageView)); }

		// Vulkan specific
		VkImage GetVkImage() const { return m_Image; }
		VkImageView GetVkImageView() const { return m_ImageView; }
		VkDeviceMemory GetVkMemory() const { return m_Memory; }
		VkFormat GetVkFormat() const { return m_VkFormat; }

	protected:
		void OnDebugNameChanged() override;

	private:
		void CreateImage(const void* data);
		void DestroyImage();
		void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

		VulkanRHIDevice* m_Device = nullptr;
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkFormat m_VkFormat = VK_FORMAT_R8G8B8A8_SRGB;
		VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	// ============================================================================
	// VULKAN RHI TEXTURE CUBE
	// ============================================================================

	class VulkanRHITextureCube : public RHITextureCube {
	public:
		VulkanRHITextureCube(VulkanRHIDevice* device, const TextureDesc& desc);
		virtual ~VulkanRHITextureCube();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Image); }
		bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;
		void SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel = 0) override;

		// Binding
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;

		// Vulkan specific
		VkImage GetVkImage() const { return m_Image; }
		VkImageView GetVkImageView() const { return m_ImageView; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkFormat m_VkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	};

	// ============================================================================
	// VULKAN RHI SAMPLER
	// ============================================================================

	class VulkanRHISampler : public RHISampler {
	public:
		VulkanRHISampler(VulkanRHIDevice* device, const SamplerState& state);
		virtual ~VulkanRHISampler();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Sampler); }
		bool IsValid() const override { return m_Sampler != VK_NULL_HANDLE; }

		// Binding
		void Bind(uint32_t slot) const override;
		void Unbind(uint32_t slot) const override;

		// Vulkan specific
		VkSampler GetVkSampler() const { return m_Sampler; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkSampler m_Sampler = VK_NULL_HANDLE;
	};

	// ============================================================================
	// VULKAN RHI TEXTURE 2D ARRAY
	// ============================================================================

	class VulkanRHITexture2DArray : public RHITexture2DArray {
	public:
		VulkanRHITexture2DArray(VulkanRHIDevice* device, const TextureDesc& desc);
		virtual ~VulkanRHITexture2DArray();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Image); }
		bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, const TextureRegion& region) override;
		void GetData(void* data, uint64_t size, const TextureRegion& region) const override;
		void GenerateMipmaps() override;
		void SetLayerData(uint32_t layer, const void* data, uint64_t size, uint32_t mipLevel = 0) override;

		// Binding
		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;
		void BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel = 0) const override;

		// Vulkan specific
		VkImage GetVkImage() const { return m_Image; }
		VkImageView GetVkImageView() const { return m_ImageView; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkFormat m_VkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	};

} // namespace RHI
} // namespace Lunex
