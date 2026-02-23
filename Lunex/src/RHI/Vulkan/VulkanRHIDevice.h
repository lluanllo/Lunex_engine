/**
 * @file VulkanRHIDevice.h
 * @brief Vulkan implementation of RHIDevice
 */

#pragma once

#include "RHI/RHIDevice.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHIDevice : public RHIDevice {
	public:
		VulkanRHIDevice(VulkanRHIContext* context);
		virtual ~VulkanRHIDevice();

		// ============================================
		// DEVICE INFO
		// ============================================

		GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
		const DeviceCapabilities& GetCapabilities() const override { return m_Capabilities; }
		const std::string& GetDeviceName() const override { return m_DeviceName; }

		// ============================================
		// RESOURCE CREATION
		// ============================================

		Ref<RHIBuffer> CreateBuffer(const BufferCreateInfo& info) override;
		Ref<RHITexture2D> CreateTexture2D(const TextureCreateInfo& info) override;
		Ref<RHITextureCube> CreateTextureCube(const TextureCreateInfo& info) override;
		Ref<RHISampler> CreateSampler(const SamplerCreateInfo& info) override;
		Ref<RHIShader> CreateShader(const ShaderCreateInfo& info) override;
		Ref<RHIPipeline> CreatePipeline(const PipelineCreateInfo& info) override;
		Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferCreateInfo& info) override;
		Ref<RHICommandList> CreateCommandList(const CommandListCreateInfo& info) override;
		Ref<RHIFence> CreateFence(bool signaled = false) override;

		// ============================================
		// MEMORY MANAGEMENT
		// ============================================

		uint64_t GetAllocatedMemory() const override { return m_AllocatedMemory; }
		const RenderStatistics& GetStatistics() const override { return m_Statistics; }
		void ResetStatistics() override { m_Statistics.Reset(); }

		// ============================================
		// DEVICE LIFETIME
		// ============================================

		void WaitIdle() override;
		void BeginFrame() override;
		void EndFrame() override;

		// ============================================
		// VULKAN SPECIFIC
		// ============================================

		VkDevice GetVkDevice() const { return m_VkDevice; }
		VkPhysicalDevice GetVkPhysicalDevice() const { return m_VkPhysicalDevice; }
		VulkanRHIContext* GetVulkanContext() const { return m_Context; }

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

		void TrackAllocation(uint64_t bytes) { m_AllocatedMemory += bytes; }
		void TrackDeallocation(uint64_t bytes) {
			if (m_AllocatedMemory >= bytes) m_AllocatedMemory -= bytes;
		}

		RenderStatistics& GetMutableStatistics() { return m_Statistics; }

	private:
		void QueryCapabilities();

		VulkanRHIContext* m_Context = nullptr;
		VkDevice m_VkDevice = VK_NULL_HANDLE;
		VkPhysicalDevice m_VkPhysicalDevice = VK_NULL_HANDLE;

		DeviceCapabilities m_Capabilities;
		std::string m_DeviceName;
		uint64_t m_AllocatedMemory = 0;
		RenderStatistics m_Statistics;

		VkPhysicalDeviceProperties m_DeviceProperties{};
		VkPhysicalDeviceFeatures m_DeviceFeatures{};
		VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
	};

} // namespace RHI
} // namespace Lunex
