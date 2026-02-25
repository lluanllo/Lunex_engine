/**
 * @file VulkanRHIContext.cpp
 * @brief Vulkan implementation of RHIContext
 */

#include "stpch.h"
#include "VulkanRHIContext.h"
#include "Log/Log.h"

#include <GLFW/glfw3.h>
#include <set>
#include <algorithm>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// PROXY FUNCTIONS FOR DEBUG UTILS
	// ============================================================================

	static VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	// ============================================================================
	// VULKAN SWAPCHAIN IMPLEMENTATION
	// ============================================================================

	VulkanSwapchain::VulkanSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
									 VkSurfaceKHR surface, GLFWwindow* window,
									 const SwapchainCreateInfo& info,
									 const QueueFamilyIndices& queueFamilies,
									 VkQueue presentQueue)
		: m_Device(device)
		, m_PhysicalDevice(physicalDevice)
		, m_Surface(surface)
		, m_Window(window)
		, m_Width(info.Width)
		, m_Height(info.Height)
		, m_VSync(info.VSync)
		, m_QueueFamilies(queueFamilies)
		, m_PresentQueue(presentQueue)
	{
		CreateSwapchain();
		CreateImageViews();
		CreateSyncObjects();
	}

	VulkanSwapchain::~VulkanSwapchain() {
		Cleanup();
	}

	void VulkanSwapchain::CreateSwapchain() {
		SwapchainSupportDetails swapchainSupport;
		{
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &swapchainSupport.Capabilities);

			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
			if (formatCount > 0) {
				swapchainSupport.Formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, swapchainSupport.Formats.data());
			}

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
			if (presentModeCount > 0) {
				swapchainSupport.PresentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, swapchainSupport.PresentModes.data());
			}
		}

		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapchainSupport.Formats);
		VkPresentModeKHR presentMode = ChoosePresentMode(swapchainSupport.PresentModes);
		VkExtent2D extent = ChooseExtent(swapchainSupport.Capabilities);

		uint32_t imageCount = swapchainSupport.Capabilities.minImageCount + 1;
		if (swapchainSupport.Capabilities.maxImageCount > 0 &&
			imageCount > swapchainSupport.Capabilities.maxImageCount) {
			imageCount = swapchainSupport.Capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t queueFamilyIndices[] = {
			m_QueueFamilies.GraphicsFamily.value(),
			m_QueueFamilies.PresentFamily.value()
		};

		if (m_QueueFamilies.GraphicsFamily != m_QueueFamilies.PresentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkResult result = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain);
		if (result != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan swapchain! VkResult: {0}", static_cast<int>(result));
			return;
		}

		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data());

		m_VkFormat = surfaceFormat.format;
		m_Width = extent.width;
		m_Height = extent.height;
	}

	void VulkanSwapchain::CreateImageViews() {
		m_ImageViews.resize(m_Images.size());
		for (size_t i = 0; i < m_Images.size(); i++) {
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_Images[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_VkFormat;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VkResult result = vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageViews[i]);
			if (result != VK_SUCCESS) {
				LNX_LOG_ERROR("Failed to create Vulkan image view {0}!", i);
			}
		}
	}

	void VulkanSwapchain::CreateSyncObjects() {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan sync objects!");
		}
	}

	void VulkanSwapchain::Cleanup() {
		if (m_Device == VK_NULL_HANDLE) return;

		vkDeviceWaitIdle(m_Device);

		if (m_InFlightFence != VK_NULL_HANDLE)
			vkDestroyFence(m_Device, m_InFlightFence, nullptr);
		if (m_RenderFinishedSemaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(m_Device, m_RenderFinishedSemaphore, nullptr);
		if (m_ImageAvailableSemaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(m_Device, m_ImageAvailableSemaphore, nullptr);

		for (auto imageView : m_ImageViews)
			vkDestroyImageView(m_Device, imageView, nullptr);

		if (m_Swapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

		m_Swapchain = VK_NULL_HANDLE;
	}

	VkSurfaceFormatKHR VulkanSwapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}
		return formats[0];
	}

	VkPresentModeKHR VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
		if (!m_VSync) {
			for (const auto& mode : modes) {
				if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
			}
			for (const auto& mode : modes) {
				if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) return mode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != UINT32_MAX) {
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width,
			capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height,
			capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}

	uint32_t VulkanSwapchain::AcquireNextImage() {
		vkWaitForFences(m_Device, 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Device, 1, &m_InFlightFence);

		vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
			m_ImageAvailableSemaphore, VK_NULL_HANDLE, &m_CurrentImageIndex);
		return m_CurrentImageIndex;
	}

	void VulkanSwapchain::Present() {
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphore };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { m_Swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &m_CurrentImageIndex;

		vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	}

	void VulkanSwapchain::Resize(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;

		vkDeviceWaitIdle(m_Device);

		// Cleanup old swapchain
		for (auto imageView : m_ImageViews)
			vkDestroyImageView(m_Device, imageView, nullptr);

		VkSwapchainKHR oldSwapchain = m_Swapchain;
		m_Swapchain = VK_NULL_HANDLE;

		CreateSwapchain();
		CreateImageViews();

		if (oldSwapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);
	}

	Ref<RHITexture2D> VulkanSwapchain::GetBackbuffer(uint32_t index) const {
		// TODO: Wrap VkImage in RHITexture2D
		return nullptr;
	}

	Ref<RHIFramebuffer> VulkanSwapchain::GetCurrentFramebuffer() const {
		// TODO: Return framebuffer for current swapchain image
		return nullptr;
	}

	void VulkanSwapchain::SetVSync(bool enabled) {
		m_VSync = enabled;
		// TODO: Recreate swapchain with new present mode
	}

	void VulkanSwapchain::SetPresentMode(PresentMode mode) {
		m_PresentMode = mode;
		m_VSync = (mode != PresentMode::Immediate);
		// TODO: Recreate swapchain
	}

	// ============================================================================
	// VULKAN RHI CONTEXT IMPLEMENTATION
	// ============================================================================

	VulkanRHIContext::VulkanRHIContext(void* windowHandle)
		: m_Window(static_cast<GLFWwindow*>(windowHandle))
	{
	}

	VulkanRHIContext::~VulkanRHIContext() {
		Shutdown();
	}

	bool VulkanRHIContext::Initialize() {
		if (m_Initialized) return true;

		LNX_LOG_INFO("Initializing Vulkan RHI Context...");

		try {
			CreateInstance();
			if (s_EnableValidation) SetupDebugMessenger();
			CreateSurface();
			PickPhysicalDevice();
			CreateLogicalDevice();
			CreateCommandPool();
		} catch (const std::exception& e) {
			LNX_LOG_ERROR("Vulkan initialization failed: {0}", e.what());
			Shutdown();
			return false;
		}

		m_Initialized = true;
		s_Instance = this;

		LNX_LOG_INFO("Vulkan RHI Context Initialized");
		LNX_LOG_INFO("  API Version: {0}", m_APIVersion);
		LNX_LOG_INFO("  Device: {0}", m_DeviceProperties.deviceName);

		return true;
	}

	void VulkanRHIContext::Shutdown() {
		if (!m_Initialized) return;

		if (m_Device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_Device);
		}

		if (m_CommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
			m_CommandPool = VK_NULL_HANDLE;
		}

		if (m_Device != VK_NULL_HANDLE) {
			vkDestroyDevice(m_Device, nullptr);
			m_Device = VK_NULL_HANDLE;
		}

		if (m_Surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
			m_Surface = VK_NULL_HANDLE;
		}

		if (s_EnableValidation && m_DebugMessenger != VK_NULL_HANDLE) {
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
			m_DebugMessenger = VK_NULL_HANDLE;
		}

		if (m_Instance != VK_NULL_HANDLE) {
			vkDestroyInstance(m_Instance, nullptr);
			m_Instance = VK_NULL_HANDLE;
		}

		if (s_Instance == this) {
			s_Instance = nullptr;
		}

		m_Initialized = false;
		LNX_LOG_INFO("Vulkan RHI Context shutdown");
	}

	void VulkanRHIContext::MakeCurrent() {
		// Vulkan doesn't have a "current context" concept like OpenGL
	}

	Ref<RHISwapchain> VulkanRHIContext::CreateSwapchain(const SwapchainCreateInfo& info) {
		return CreateRef<VulkanSwapchain>(
			m_Device, m_PhysicalDevice, m_Surface, m_Window,
			info, m_QueueFamilies, m_PresentQueue);
	}

	std::string VulkanRHIContext::GetAPIVersion() const {
		return m_APIVersion;
	}

	void VulkanRHIContext::EnableDebugOutput(bool enable) {
		m_DebugEnabled = enable;
	}

	void VulkanRHIContext::PushDebugGroup(const std::string& name) {
		// TODO: Use VK_EXT_debug_utils
	}

	void VulkanRHIContext::PopDebugGroup() {
		// TODO: Use VK_EXT_debug_utils
	}

	void VulkanRHIContext::InsertDebugMarker(const std::string& name) {
		// TODO: Use VK_EXT_debug_utils
	}

	SwapchainSupportDetails VulkanRHIContext::QuerySwapchainSupport() const {
		return QuerySwapchainSupport(m_PhysicalDevice);
	}

	VkCommandBuffer VulkanRHIContext::BeginSingleTimeCommands() const {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void VulkanRHIContext::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}

	// ============================================================================
	// VULKAN INITIALIZATION STEPS
	// ============================================================================

	void VulkanRHIContext::CreateInstance() {
		if (s_EnableValidation && !CheckValidationLayerSupport()) {
			LNX_LOG_WARN("Vulkan validation layers requested but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Lunex Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Lunex";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (s_EnableValidation) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(std::size(s_ValidationLayers));
			createInfo.ppEnabledLayerNames = s_ValidationLayers;

			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = DebugCallback;
			createInfo.pNext = &debugCreateInfo;
		} else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
		if (result != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to create Vulkan instance! VkResult: {0}", static_cast<int>(result));
			throw std::runtime_error("Failed to create Vulkan instance");
		}

		// Store API version
		uint32_t apiVersion;
		vkEnumerateInstanceVersion(&apiVersion);
		m_APIVersion = std::to_string(VK_VERSION_MAJOR(apiVersion)) + "." +
					   std::to_string(VK_VERSION_MINOR(apiVersion)) + "." +
					   std::to_string(VK_VERSION_PATCH(apiVersion));
	}

	void VulkanRHIContext::SetupDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;

		VkResult result = CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
		if (result != VK_SUCCESS) {
			LNX_LOG_WARN("Failed to set up Vulkan debug messenger");
		}
	}

	void VulkanRHIContext::CreateSurface() {
		if (!m_Window) {
			throw std::runtime_error("No window handle for Vulkan surface creation");
		}

		VkResult result = glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Vulkan window surface");
		}
	}

	void VulkanRHIContext::PickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error("No Vulkan-capable GPU found!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

		// Pick best device (prefer discrete GPU)
		VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
		int bestScore = -1;

		for (const auto& device : devices) {
			if (!IsDeviceSuitable(device)) continue;

			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);

			int score = 0;
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
			score += static_cast<int>(props.limits.maxImageDimension2D);

			if (score > bestScore) {
				bestScore = score;
				bestDevice = device;
			}
		}

		if (bestDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Failed to find a suitable Vulkan GPU!");
		}

		m_PhysicalDevice = bestDevice;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties);
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_DeviceFeatures);
		m_QueueFamilies = FindQueueFamilies(m_PhysicalDevice);

		LNX_LOG_INFO("Vulkan Physical Device: {0}", m_DeviceProperties.deviceName);
		LNX_LOG_INFO("  Driver Version: {0}.{1}.{2}",
			VK_VERSION_MAJOR(m_DeviceProperties.driverVersion),
			VK_VERSION_MINOR(m_DeviceProperties.driverVersion),
			VK_VERSION_PATCH(m_DeviceProperties.driverVersion));
	}

	void VulkanRHIContext::CreateLogicalDevice() {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {
			m_QueueFamilies.GraphicsFamily.value(),
			m_QueueFamilies.PresentFamily.value()
		};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		deviceFeatures.wideLines = VK_TRUE;
		deviceFeatures.geometryShader = m_DeviceFeatures.geometryShader;
		deviceFeatures.tessellationShader = m_DeviceFeatures.tessellationShader;
		deviceFeatures.multiDrawIndirect = m_DeviceFeatures.multiDrawIndirect;
		deviceFeatures.independentBlend = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(s_DeviceExtensions));
		createInfo.ppEnabledExtensionNames = s_DeviceExtensions;

		if (s_EnableValidation) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(std::size(s_ValidationLayers));
			createInfo.ppEnabledLayerNames = s_ValidationLayers;
		} else {
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Vulkan logical device");
		}

		vkGetDeviceQueue(m_Device, m_QueueFamilies.GraphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_QueueFamilies.PresentFamily.value(), 0, &m_PresentQueue);

		if (m_QueueFamilies.ComputeFamily.has_value()) {
			vkGetDeviceQueue(m_Device, m_QueueFamilies.ComputeFamily.value(), 0, &m_ComputeQueue);
		}
	}

	void VulkanRHIContext::CreateCommandPool() {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_QueueFamilies.GraphicsFamily.value();

		VkResult result = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Vulkan command pool");
		}
	}

	// ============================================================================
	// HELPER FUNCTIONS
	// ============================================================================

	QueueFamilyIndices VulkanRHIContext::FindQueueFamilies(VkPhysicalDevice device) const {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.GraphicsFamily = i;
			}

			if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				indices.ComputeFamily = i;
			}

			if ((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
				!(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				indices.TransferFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
			if (presentSupport) {
				indices.PresentFamily = i;
			}

			if (indices.IsComplete()) break;
		}

		return indices;
	}

	SwapchainSupportDetails VulkanRHIContext::QuerySwapchainSupport(VkPhysicalDevice device) const {
		SwapchainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
		if (formatCount > 0) {
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.Formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
		if (presentModeCount > 0) {
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	bool VulkanRHIContext::IsDeviceSuitable(VkPhysicalDevice device) const {
		QueueFamilyIndices indices = FindQueueFamilies(device);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapchainSupportDetails support = QuerySwapchainSupport(device);
			swapChainAdequate = !support.Formats.empty() && !support.PresentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.IsComplete() && extensionsSupported && swapChainAdequate &&
			   supportedFeatures.samplerAnisotropy;
	}

	bool VulkanRHIContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) const {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(
			std::begin(s_DeviceExtensions), std::end(s_DeviceExtensions));

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	std::vector<const char*> VulkanRHIContext::GetRequiredExtensions() const {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (s_EnableValidation) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	bool VulkanRHIContext::CheckValidationLayerSupport() const {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : s_ValidationLayers) {
			bool found = false;
			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) return false;
		}
		return true;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRHIContext::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			LNX_LOG_ERROR("[Vulkan Validation] {0}", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			LNX_LOG_WARN("[Vulkan Validation] {0}", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			LNX_LOG_INFO("[Vulkan Validation] {0}", pCallbackData->pMessage);
			break;
		default:
			LNX_LOG_TRACE("[Vulkan Validation] {0}", pCallbackData->pMessage);
			break;
		}

		return VK_FALSE;
	}

} // namespace RHI
} // namespace Lunex
