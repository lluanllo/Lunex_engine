/**
 * @file VulkanRHIContext.h
 * @brief Vulkan implementation of RHIContext
 * 
 * Manages VkInstance, VkPhysicalDevice, VkSurface, and debug messenger.
 * The context is the entry point for all Vulkan operations.
 */

#pragma once

#include "RHI/RHIContext.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <optional>

struct GLFWwindow;

namespace Lunex {
namespace RHI {

	// ============================================================================
	// QUEUE FAMILY INDICES
	// ============================================================================

	struct QueueFamilyIndices {
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;
		std::optional<uint32_t> ComputeFamily;
		std::optional<uint32_t> TransferFamily;

		bool IsComplete() const {
			return GraphicsFamily.has_value() && PresentFamily.has_value();
		}
	};

	// ============================================================================
	// SWAPCHAIN SUPPORT DETAILS
	// ============================================================================

	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR Capabilities{};
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	// ============================================================================
	// VULKAN SWAPCHAIN
	// ============================================================================

	class VulkanSwapchain : public RHISwapchain {
	public:
		VulkanSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
						VkSurfaceKHR surface, GLFWwindow* window,
						const SwapchainCreateInfo& info,
						const QueueFamilyIndices& queueFamilies,
						VkQueue presentQueue);
		virtual ~VulkanSwapchain();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Swapchain); }
		bool IsValid() const override { return m_Swapchain != VK_NULL_HANDLE; }

		// Swapchain operations
		uint32_t AcquireNextImage() override;
		void Present() override;
		void Resize(uint32_t width, uint32_t height) override;

		// Properties
		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		TextureFormat GetFormat() const override { return m_Format; }
		uint32_t GetBufferCount() const override { return static_cast<uint32_t>(m_Images.size()); }
		uint32_t GetCurrentBufferIndex() const override { return m_CurrentImageIndex; }

		Ref<RHITexture2D> GetBackbuffer(uint32_t index) const override;
		Ref<RHIFramebuffer> GetCurrentFramebuffer() const override;

		// VSync
		void SetVSync(bool enabled) override;
		bool IsVSyncEnabled() const override { return m_VSync; }
		void SetPresentMode(PresentMode mode) override;
		PresentMode GetPresentMode() const override { return m_PresentMode; }

		// Vulkan specific
		VkSwapchainKHR GetVkSwapchain() const { return m_Swapchain; }
		VkFormat GetVkFormat() const { return m_VkFormat; }
		VkExtent2D GetExtent() const { return { m_Width, m_Height }; }
		const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }
		VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphore; }
		VkSemaphore GetRenderFinishedSemaphore() const { return m_RenderFinishedSemaphore; }

	private:
		void CreateSwapchain();
		void CreateImageViews();
		void CreateSyncObjects();
		void Cleanup();

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
		VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities);

		VkDevice m_Device = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		GLFWwindow* m_Window = nullptr;
		QueueFamilyIndices m_QueueFamilies;

		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		VkFormat m_VkFormat = VK_FORMAT_B8G8R8A8_SRGB;
		TextureFormat m_Format = TextureFormat::RGBA8;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_VSync = true;
		PresentMode m_PresentMode = PresentMode::VSync;

		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		uint32_t m_CurrentImageIndex = 0;

		VkSemaphore m_ImageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore m_RenderFinishedSemaphore = VK_NULL_HANDLE;
		VkFence m_InFlightFence = VK_NULL_HANDLE;

		VkQueue m_PresentQueue = VK_NULL_HANDLE;
	};

	// ============================================================================
	// VULKAN RHI CONTEXT
	// ============================================================================

	class VulkanRHIContext : public RHIContext {
	public:
		VulkanRHIContext(void* windowHandle);
		virtual ~VulkanRHIContext();

		// Lifecycle
		bool Initialize() override;
		void Shutdown() override;
		void MakeCurrent() override;

		// Swapchain
		Ref<RHISwapchain> CreateSwapchain(const SwapchainCreateInfo& info) override;

		// Info
		GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
		std::string GetAPIVersion() const override;
		bool IsValid() const override { return m_Initialized; }

		// Debug
		void EnableDebugOutput(bool enable) override;
		void PushDebugGroup(const std::string& name) override;
		void PopDebugGroup() override;
		void InsertDebugMarker(const std::string& name) override;

		// Vulkan specific accessors
		VkInstance GetInstance() const { return m_Instance; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetDevice() const { return m_Device; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }
		VkQueue GetComputeQueue() const { return m_ComputeQueue; }
		VkCommandPool GetCommandPool() const { return m_CommandPool; }
		const QueueFamilyIndices& GetQueueFamilies() const { return m_QueueFamilies; }
		SwapchainSupportDetails QuerySwapchainSupport() const;

		// Single-use command buffer helpers
		VkCommandBuffer BeginSingleTimeCommands() const;
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;

	private:
		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateCommandPool();

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
		SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) const;
		bool IsDeviceSuitable(VkPhysicalDevice device) const;
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
		std::vector<const char*> GetRequiredExtensions() const;
		bool CheckValidationLayerSupport() const;

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

		GLFWwindow* m_Window = nullptr;
		bool m_Initialized = false;
		bool m_DebugEnabled = false;

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkQueue m_ComputeQueue = VK_NULL_HANDLE;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		QueueFamilyIndices m_QueueFamilies;

		VkPhysicalDeviceProperties m_DeviceProperties{};
		VkPhysicalDeviceFeatures m_DeviceFeatures{};

		std::string m_APIVersion;

		static constexpr const char* s_ValidationLayers[] = {
			"VK_LAYER_KHRONOS_validation"
		};

		static constexpr const char* s_DeviceExtensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

#ifdef LN_DEBUG
		static constexpr bool s_EnableValidation = true;
#else
		static constexpr bool s_EnableValidation = false;
#endif
	};

} // namespace RHI
} // namespace Lunex
