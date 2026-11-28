#include "stpch.h"
#include "VulkanContext.h"

#include "Log/Log.h"

#include <GLFW/glfw3.h>
#include "backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <cstring>

namespace Lunex::RHI::Vk {

	static VulkanContext* s_Context = nullptr;
	static constexpr VkDeviceSize kBlockSize = 64ull * 1024 * 1024;

	VulkanContext& VulkanContext::Get() {
		if (!s_Context) s_Context = new VulkanContext();   // lives for the whole process
		return *s_Context;
	}

	bool VulkanContext::IsActive() { return s_Context && s_Context->m_Device != VK_NULL_HANDLE; }

	const char* ResultString(VkResult r) {
		switch (r) {
			case VK_SUCCESS: return "VK_SUCCESS";
			case VK_NOT_READY: return "VK_NOT_READY";
			case VK_TIMEOUT: return "VK_TIMEOUT";
			case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
			case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
			case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
			case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
			case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
			case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
			case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
			case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
			case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
			case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
			default: return "VK_ERROR";
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
		const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
		if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) LNX_LOG_ERROR("[Vulkan] {0}", data->pMessage);
		else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) LNX_LOG_WARN("[Vulkan] {0}", data->pMessage);
		return VK_FALSE;
	}

	// ============================================================================
	// INIT
	// ============================================================================

	bool VulkanContext::Init(GLFWwindow* window) {
		m_Window = window;
		m_Validation = std::getenv("LUNEX_VULKAN_VALIDATION") != nullptr;
		if (!CreateInstance()) return false;
		if (glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface) != VK_SUCCESS) {
			LNX_LOG_ERROR("Vulkan: could not create the window surface");
			return false;
		}
		if (!PickPhysicalDevice() || !CreateDevice()) return false;

		for (auto& f : m_Frames) {
			VkCommandPoolCreateInfo pool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			pool.queueFamilyIndex = m_QueueFamily;
			LNX_VK_CHECK(vkCreateCommandPool(m_Device, &pool, nullptr, &f.Pool));
			VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			alloc.commandPool = f.Pool;
			alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc.commandBufferCount = 1;
			LNX_VK_CHECK(vkAllocateCommandBuffers(m_Device, &alloc, &f.Cmd));
			VkFenceCreateInfo fence{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			LNX_VK_CHECK(vkCreateFence(m_Device, &fence, nullptr, &f.InFlight));
			VkSemaphoreCreateInfo sem{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			LNX_VK_CHECK(vkCreateSemaphore(m_Device, &sem, nullptr, &f.ImageAvailable));
		}
		VkCommandPoolCreateInfo pool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		pool.queueFamilyIndex = m_QueueFamily;
		LNX_VK_CHECK(vkCreateCommandPool(m_Device, &pool, nullptr, &m_UploadPool));
		VkFenceCreateInfo fence{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		LNX_VK_CHECK(vkCreateFence(m_Device, &fence, nullptr, &m_UploadFence));

		if (!CreateSwapchain()) return false;
		LNX_LOG_INFO("Vulkan initialized: {0} (API {1}.{2}.{3}){4}", m_DeviceName, VK_API_VERSION_MAJOR(m_Properties.apiVersion),
			VK_API_VERSION_MINOR(m_Properties.apiVersion), VK_API_VERSION_PATCH(m_Properties.apiVersion), m_Validation ? " + validation" : "");
		return true;
	}

	bool VulkanContext::CreateInstance() {
		VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app.pApplicationName = "Lunex Editor";
		app.pEngineName = "Lunex";
		app.apiVersion = VK_API_VERSION_1_3;

		uint32_t glfwCount = 0;
		const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
		std::vector<const char*> extensions(glfwExt, glfwExt + glfwCount);
		std::vector<const char*> layers;
		if (m_Validation) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			layers.push_back("VK_LAYER_KHRONOS_validation");
		}
		VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		info.pApplicationInfo = &app;
		info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		info.ppEnabledExtensionNames = extensions.data();
		info.enabledLayerCount = static_cast<uint32_t>(layers.size());
		info.ppEnabledLayerNames = layers.data();
		VkResult r = vkCreateInstance(&info, nullptr, &m_Instance);
		if (r != VK_SUCCESS && m_Validation) {
			LNX_LOG_WARN("Vulkan: validation layer unavailable, continuing without it");
			m_Validation = false;
			extensions.pop_back();
			info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			info.enabledLayerCount = 0;
			r = vkCreateInstance(&info, nullptr, &m_Instance);
		}
		if (r != VK_SUCCESS) { LNX_LOG_ERROR("Vulkan: vkCreateInstance failed ({0})", ResultString(r)); return false; }

		if (m_Validation) {
			auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
			if (create) {
				VkDebugUtilsMessengerCreateInfoEXT dbg{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
				dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
				dbg.pfnUserCallback = DebugCallback;
				create(m_Instance, &dbg, nullptr, &m_Messenger);
			}
		}
		return true;
	}

	bool VulkanContext::PickPhysicalDevice() {
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
		if (count == 0) { LNX_LOG_ERROR("Vulkan: no GPU with Vulkan support"); return false; }
		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

		int bestScore = -1;
		for (VkPhysicalDevice d : devices) {
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(d, &p);
			if (p.apiVersion < VK_API_VERSION_1_3) continue;
			uint32_t qCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
			std::vector<VkQueueFamilyProperties> qs(qCount);
			vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qs.data());
			int family = -1;
			for (uint32_t i = 0; i < qCount; i++) {
				VkBool32 present = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(d, i, m_Surface, &present);
				if ((qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && present) { family = static_cast<int>(i); break; }
			}
			if (family < 0) continue;
			int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 100 : 10;
			if (score > bestScore) { bestScore = score; m_PhysicalDevice = d; m_QueueFamily = static_cast<uint32_t>(family); }
		}
		if (!m_PhysicalDevice) { LNX_LOG_ERROR("Vulkan: no GPU supports Vulkan 1.3 + presentation"); return false; }
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_Properties);
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);
		m_DeviceName = m_Properties.deviceName;
		return true;
	}

	bool VulkanContext::CreateDevice() {
		uint32_t extCount = 0;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, nullptr);
		std::vector<VkExtensionProperties> available(extCount);
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, available.data());
		auto has = [&](const char* name) {
			return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
		};
		std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		// GL clip space (z in [-1, 1]) without touching the engine's projection matrices
		m_DepthClipControl = has(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
		if (m_DepthClipControl) extensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);

		VkPhysicalDeviceFeatures supported{};
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &supported);
		VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features.features.samplerAnisotropy = supported.samplerAnisotropy;
		features.features.fillModeNonSolid = supported.fillModeNonSolid;
		features.features.wideLines = supported.wideLines;
		features.features.independentBlend = supported.independentBlend;
		features.features.geometryShader = supported.geometryShader;
		features.features.depthClamp = supported.depthClamp;
		features.features.imageCubeArray = supported.imageCubeArray;
		features.features.shaderStorageImageWriteWithoutFormat = supported.shaderStorageImageWriteWithoutFormat;
		VkPhysicalDeviceVulkan13Features v13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		v13.dynamicRendering = VK_TRUE;
		v13.synchronization2 = VK_TRUE;
		VkPhysicalDeviceVulkan12Features v12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		v12.scalarBlockLayout = VK_TRUE;
		VkPhysicalDeviceDepthClipControlFeaturesEXT clip{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT };
		clip.depthClipControl = VK_TRUE;
		features.pNext = &v13;
		v13.pNext = &v12;
		v12.pNext = m_DepthClipControl ? &clip : nullptr;

		const float priority = 1.0f;
		VkDeviceQueueCreateInfo queue{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue.queueFamilyIndex = m_QueueFamily;
		queue.queueCount = 1;
		queue.pQueuePriorities = &priority;
		VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		info.pNext = &features;
		info.queueCreateInfoCount = 1;
		info.pQueueCreateInfos = &queue;
		info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		info.ppEnabledExtensionNames = extensions.data();
		const VkResult r = vkCreateDevice(m_PhysicalDevice, &info, nullptr, &m_Device);
		if (r != VK_SUCCESS) { LNX_LOG_ERROR("Vulkan: vkCreateDevice failed ({0})", ResultString(r)); return false; }
		vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
		return true;
	}

	// ============================================================================
	// SWAPCHAIN
	// ============================================================================

	bool VulkanContext::CreateSwapchain() {
		VkSurfaceCapabilitiesKHR caps;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);
		int w = 0, h = 0;
		glfwGetFramebufferSize(m_Window, &w, &h);
		if (w == 0 || h == 0) return false;
		VkExtent2D extent = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent
			: VkExtent2D{ std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width),
			              std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height) };
		if (extent.width == 0 || extent.height == 0) return false;

		uint32_t fmtCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &fmtCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(fmtCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &fmtCount, formats.data());
		// UNORM like the GL default framebuffer: the engine applies gamma itself
		VkSurfaceFormatKHR chosen = formats[0];
		for (const auto& f : formats)
			if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }

		uint32_t pmCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &pmCount, nullptr);
		std::vector<VkPresentModeKHR> modes(pmCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &pmCount, modes.data());
		VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
		// VSync off: IMMEDIATE never waits for the compositor (like swap interval 0 in GL). MAILBOX in a
		// window is paced by DWM on Windows: vkAcquireNextImageKHR blocks until it releases an image.
		if (!m_VSync) {
			if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) mode = VK_PRESENT_MODE_MAILBOX_KHR;
		}

		uint32_t imageCount = std::max(caps.minImageCount + 1, mode == VK_PRESENT_MODE_FIFO_KHR ? 3u : 4u);
		if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

		VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		info.surface = m_Surface;
		info.minImageCount = imageCount;
		info.imageFormat = chosen.format;
		info.imageColorSpace = chosen.colorSpace;
		info.imageExtent = extent;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.preTransform = caps.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = mode;
		info.clipped = VK_TRUE;
		info.oldSwapchain = m_Swapchain;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		if (vkCreateSwapchainKHR(m_Device, &info, nullptr, &swapchain) != VK_SUCCESS) { LNX_LOG_ERROR("Vulkan: swapchain creation failed"); return false; }
		if (m_Swapchain) {
			for (auto v : m_SwapchainViews) vkDestroyImageView(m_Device, v, nullptr);
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		}
		m_Swapchain = swapchain;
		m_SwapchainFormat = chosen.format;
		m_SwapchainExtent = extent;

		uint32_t count = 0;
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, nullptr);
		m_SwapchainImages.resize(count);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, m_SwapchainImages.data());
		m_SwapchainViews.resize(count);
		for (uint32_t i = 0; i < count; i++) {
			VkImageViewCreateInfo view{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			view.image = m_SwapchainImages[i];
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.format = m_SwapchainFormat;
			view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			LNX_VK_CHECK(vkCreateImageView(m_Device, &view, nullptr, &m_SwapchainViews[i]));
		}
		for (auto s : m_RenderFinished) vkDestroySemaphore(m_Device, s, nullptr);
		m_RenderFinished.resize(count);
		for (auto& s : m_RenderFinished) {
			VkSemaphoreCreateInfo sem{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			vkCreateSemaphore(m_Device, &sem, nullptr, &s);
		}
		m_SwapchainDirty = false;
		if (m_ImGuiReady) ImGui_ImplVulkan_SetMinImageCount(std::max(2u, caps.minImageCount));
		return true;
	}

	void VulkanContext::DestroySwapchain() {
		for (auto v : m_SwapchainViews) vkDestroyImageView(m_Device, v, nullptr);
		m_SwapchainViews.clear();
		for (auto s : m_RenderFinished) vkDestroySemaphore(m_Device, s, nullptr);
		m_RenderFinished.clear();
		if (m_Swapchain) vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		m_Swapchain = VK_NULL_HANDLE;
	}

	void VulkanContext::RecreateSwapchain() {
		vkDeviceWaitIdle(m_Device);
		CreateSwapchain();
	}

	void VulkanContext::OnResize(uint32_t, uint32_t) { m_SwapchainDirty = true; }

	void VulkanContext::SetVSync(bool enabled) {
		if (enabled == m_VSync) return;
		m_VSync = enabled;
		m_SwapchainDirty = true;
	}

	// ============================================================================
	// FRAME
	// ============================================================================

	bool VulkanContext::BeginFrame() {
		if (m_FrameActive) return true;
		if (m_SwapchainDirty || !m_Swapchain) {
			int w = 0, h = 0;
			glfwGetFramebufferSize(m_Window, &w, &h);
			if (w == 0 || h == 0) return false;
			RecreateSwapchain();
			if (!m_Swapchain) return false;
		}
		FrameData& frame = m_Frames[m_FrameIndex];
		vkWaitForFences(m_Device, 1, &frame.InFlight, VK_TRUE, UINT64_MAX);
		FlushDeletions(false);

		VkResult r = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, frame.ImageAvailable, VK_NULL_HANDLE, &m_ImageIndex);
		if (r == VK_ERROR_OUT_OF_DATE_KHR) { m_SwapchainDirty = true; return false; }
		if (r == VK_SUBOPTIMAL_KHR) m_SwapchainDirty = true;
		vkResetFences(m_Device, 1, &frame.InFlight);

		vkResetCommandBuffer(frame.Cmd, 0);
		VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(frame.Cmd, &begin);
		m_FrameActive = true;
		return true;
	}

	static void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
	                         VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
		VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		b.srcStageMask = srcStage; b.srcAccessMask = srcAccess;
		b.dstStageMask = dstStage; b.dstAccessMask = dstAccess;
		b.oldLayout = from; b.newLayout = to;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = image;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	void VulkanContext::EndFrame(const std::function<void(VkCommandBuffer)>& renderUI) {
		if (!m_FrameActive) return;
		FrameData& frame = m_Frames[m_FrameIndex];
		VkCommandBuffer cmd = frame.Cmd;
		VkImage image = m_SwapchainImages[m_ImageIndex];

		ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		VkRenderingAttachmentInfo color{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		color.imageView = m_SwapchainViews[m_ImageIndex];
		color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.clearValue.color = { { 0.08f, 0.08f, 0.08f, 1.0f } };
		VkRenderingInfo rendering{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		rendering.renderArea = { { 0, 0 }, m_SwapchainExtent };
		rendering.layerCount = 1;
		rendering.colorAttachmentCount = 1;
		rendering.pColorAttachments = &color;
		vkCmdBeginRendering(cmd, &rendering);
		if (renderUI) renderUI(cmd);
		vkCmdEndRendering(cmd);

		ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
		vkEndCommandBuffer(cmd);

		VkSemaphoreSubmitInfo wait{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		wait.semaphore = frame.ImageAvailable;
		wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSemaphoreSubmitInfo signal{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signal.semaphore = m_RenderFinished[m_ImageIndex];
		signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmdInfo.commandBuffer = cmd;
		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount = 1;
		submit.pWaitSemaphoreInfos = &wait;
		submit.commandBufferInfoCount = 1;
		submit.pCommandBufferInfos = &cmdInfo;
		submit.signalSemaphoreInfoCount = 1;
		submit.pSignalSemaphoreInfos = &signal;
		{
			std::lock_guard<std::mutex> lock(m_UploadMutex);   // the queue is shared with uploads
			LNX_VK_CHECK(vkQueueSubmit2(m_Queue, 1, &submit, frame.InFlight));
			VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
			present.waitSemaphoreCount = 1;
			present.pWaitSemaphores = &m_RenderFinished[m_ImageIndex];
			present.swapchainCount = 1;
			present.pSwapchains = &m_Swapchain;
			present.pImageIndices = &m_ImageIndex;
			const VkResult r = vkQueuePresentKHR(m_Queue, &present);
			if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) m_SwapchainDirty = true;
		}
		m_FrameActive = false;
		m_FrameIndex = (m_FrameIndex + 1) % kFramesInFlight;
		m_FrameNumber++;
	}

	void VulkanContext::FlushDeletions(bool all) {
		// Frame N waits on the fence of frame N - kFramesInFlight: everything up to it is done
		std::vector<std::function<void()>> ready;
		{
			std::lock_guard<std::mutex> lock(m_DeletionMutex);
			while (!m_Deletions.empty() && (all || m_Deletions.front().first + kFramesInFlight <= m_FrameNumber)) {
				ready.push_back(std::move(m_Deletions.front().second));
				m_Deletions.pop_front();
			}
		}
		for (auto& fn : ready) fn();
	}

	void VulkanContext::DeferDelete(std::function<void()> fn) {
		if (!m_Device) return;
		std::lock_guard<std::mutex> lock(m_DeletionMutex);
		m_Deletions.emplace_back(m_FrameNumber, std::move(fn));
	}

	void VulkanContext::WaitIdle() { if (m_Device) vkDeviceWaitIdle(m_Device); }

	void VulkanContext::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
		std::lock_guard<std::mutex> lock(m_UploadMutex);
		VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc.commandPool = m_UploadPool;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc.commandBufferCount = 1;
		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(m_Device, &alloc, &cmd);
		VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &begin);
		record(cmd);
		vkEndCommandBuffer(cmd);
		VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmdInfo.commandBuffer = cmd;
		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.commandBufferInfoCount = 1;
		submit.pCommandBufferInfos = &cmdInfo;
		vkResetFences(m_Device, 1, &m_UploadFence);
		vkQueueSubmit2(m_Queue, 1, &submit, m_UploadFence);
		vkWaitForFences(m_Device, 1, &m_UploadFence, VK_TRUE, UINT64_MAX);
		vkFreeCommandBuffers(m_Device, m_UploadPool, 1, &cmd);
	}

	// ============================================================================
	// MEMORY (block sub-allocator)
	// ============================================================================

	uint32_t VulkanContext::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
			if ((typeBits & (1u << i)) && (m_MemoryProperties.memoryTypes[i].propertyFlags & props) == props) return i;
		// Fallback: drop the optional flags
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
			if (typeBits & (1u << i)) return i;
		return 0;
	}

	Allocation VulkanContext::Allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags props) {
		std::lock_guard<std::mutex> lock(m_AllocMutex);
		Allocation a;
		a.MemoryType = FindMemoryType(req.memoryTypeBits, props);
		const bool hostVisible = (m_MemoryProperties.memoryTypes[a.MemoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
		// Granularity keeps linear (buffers) and optimal (images) resources apart
		const VkDeviceSize align = std::max<VkDeviceSize>(req.alignment, m_Properties.limits.bufferImageGranularity);
		const VkDeviceSize size = (req.size + align - 1) / align * align;

		if (size <= kBlockSize / 4) {
			for (int32_t b = 0; b < static_cast<int32_t>(m_Blocks.size()); b++) {
				Block& block = m_Blocks[b];
				if (block.MemoryType != a.MemoryType) continue;
				for (size_t f = 0; f < block.Free.size(); f++) {
					auto& [offset, freeSize] = block.Free[f];
					const VkDeviceSize aligned = (offset + align - 1) / align * align;
					const VkDeviceSize pad = aligned - offset;
					if (freeSize < pad + size) continue;
					a.Memory = block.Memory;
					a.Offset = aligned;
					a.Size = size;
					a.Block = b;
					a.Mapped = block.Mapped ? static_cast<char*>(block.Mapped) + aligned : nullptr;
					const VkDeviceSize remainOffset = aligned + size, remainSize = freeSize - pad - size;
					if (pad > 0) { freeSize = pad; if (remainSize > 0) block.Free.insert(block.Free.begin() + f + 1, { remainOffset, remainSize }); }
					else if (remainSize > 0) { offset = remainOffset; freeSize = remainSize; }
					else block.Free.erase(block.Free.begin() + f);
					return a;
				}
			}
			// New block
			Block block;
			block.MemoryType = a.MemoryType;
			block.Size = kBlockSize;
			VkMemoryAllocateInfo info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			info.allocationSize = kBlockSize;
			info.memoryTypeIndex = a.MemoryType;
			if (vkAllocateMemory(m_Device, &info, nullptr, &block.Memory) != VK_SUCCESS) { LNX_LOG_ERROR("Vulkan: out of memory (block)"); return {}; }
			if (hostVisible) vkMapMemory(m_Device, block.Memory, 0, VK_WHOLE_SIZE, 0, &block.Mapped);
			block.Free.push_back({ size, kBlockSize - size });
			m_AllocatedBytes += kBlockSize;
			m_Blocks.push_back(block);
			a.Memory = block.Memory;
			a.Offset = 0;
			a.Size = size;
			a.Block = static_cast<int32_t>(m_Blocks.size()) - 1;
			a.Mapped = block.Mapped;
			return a;
		}
		// Large resources: dedicated allocation
		VkMemoryAllocateInfo info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		info.allocationSize = req.size;
		info.memoryTypeIndex = a.MemoryType;
		if (vkAllocateMemory(m_Device, &info, nullptr, &a.Memory) != VK_SUCCESS) { LNX_LOG_ERROR("Vulkan: out of memory"); return {}; }
		if (hostVisible) vkMapMemory(m_Device, a.Memory, 0, VK_WHOLE_SIZE, 0, &a.Mapped);
		a.Size = req.size;
		m_AllocatedBytes += req.size;
		return a;
	}

	void VulkanContext::Free(const Allocation& a) {
		if (!a.IsValid() || !m_Device) return;
		std::lock_guard<std::mutex> lock(m_AllocMutex);
		if (a.Block < 0) {
			vkFreeMemory(m_Device, a.Memory, nullptr);
			m_AllocatedBytes -= a.Size;
			return;
		}
		Block& block = m_Blocks[a.Block];
		auto& free = block.Free;
		auto it = std::lower_bound(free.begin(), free.end(), std::make_pair(a.Offset, VkDeviceSize(0)));
		it = free.insert(it, { a.Offset, a.Size });
		// Merge with neighbours
		if (it + 1 != free.end() && it->first + it->second == (it + 1)->first) { it->second += (it + 1)->second; free.erase(it + 1); }
		if (it != free.begin() && (it - 1)->first + (it - 1)->second == it->first) { (it - 1)->second += it->second; free.erase(it); }
	}

	// ============================================================================
	// SAMPLERS
	// ============================================================================

	VkSampler VulkanContext::GetSampler(VkFilter filter, VkSamplerMipmapMode mip, VkSamplerAddressMode address, bool compare, float anisotropy) {
		const uint64_t key = static_cast<uint64_t>(filter) | (static_cast<uint64_t>(mip) << 4) | (static_cast<uint64_t>(address) << 8) |
			(static_cast<uint64_t>(compare) << 12) | (static_cast<uint64_t>(anisotropy) << 16);
		auto it = m_Samplers.find(key);
		if (it != m_Samplers.end()) return it->second;
		VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		info.magFilter = info.minFilter = filter;
		info.mipmapMode = mip;
		info.addressModeU = info.addressModeV = info.addressModeW = address;
		info.maxLod = VK_LOD_CLAMP_NONE;
		info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (anisotropy > 1.0f) { info.anisotropyEnable = VK_TRUE; info.maxAnisotropy = std::min(anisotropy, m_Properties.limits.maxSamplerAnisotropy); }
		if (compare) { info.compareEnable = VK_TRUE; info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; }
		VkSampler sampler = VK_NULL_HANDLE;
		vkCreateSampler(m_Device, &info, nullptr, &sampler);
		m_Samplers[key] = sampler;
		return sampler;
	}

	// ============================================================================
	// IMGUI TEXTURES
	// ============================================================================

	namespace {
		struct PendingImGuiTexture { VkImageView View; VkSampler Sampler; VkImageLayout Layout; };
		// Leaked: textures may be destroyed during static destruction
		std::unordered_map<uint32_t, PendingImGuiTexture>& s_Pending = *new std::unordered_map<uint32_t, PendingImGuiTexture>();
		std::mutex& s_ImGuiMutex = *new std::mutex();
	}

	uint32_t VulkanContext::RegisterImGuiTexture(VkImageView view, VkSampler sampler, VkImageLayout layout) {
		std::lock_guard<std::mutex> lock(s_ImGuiMutex);
		uint32_t index;
		if (!m_FreeImGuiIds.empty()) { index = m_FreeImGuiIds.back(); m_FreeImGuiIds.pop_back(); m_ImGuiTextures[index] = {}; }
		else { index = static_cast<uint32_t>(m_ImGuiTextures.size()); m_ImGuiTextures.push_back({}); }
		s_Pending[index] = { view, sampler, layout };   // descriptor set created on first use (main thread, ImGui ready)
		return kImGuiIdBase + index;
	}

	void VulkanContext::UpdateImGuiTexture(uint32_t id, VkImageView view, VkSampler sampler, VkImageLayout layout) {
		if (id < kImGuiIdBase) return;
		std::lock_guard<std::mutex> lock(s_ImGuiMutex);
		const uint32_t index = id - kImGuiIdBase;
		if (index >= m_ImGuiTextures.size()) return;
		if (m_ImGuiTextures[index].Set) {
			VkDescriptorSet old = m_ImGuiTextures[index].Set;
			DeferDelete([old] { ImGui_ImplVulkan_RemoveTexture(old); });
			m_ImGuiTextures[index].Set = VK_NULL_HANDLE;
		}
		s_Pending[index] = { view, sampler, layout };
	}

	void VulkanContext::UnregisterImGuiTexture(uint32_t id) {
		if (id < kImGuiIdBase) return;
		std::lock_guard<std::mutex> lock(s_ImGuiMutex);
		const uint32_t index = id - kImGuiIdBase;
		if (index >= m_ImGuiTextures.size()) return;
		s_Pending.erase(index);
		if (VkDescriptorSet set = m_ImGuiTextures[index].Set) DeferDelete([set] { ImGui_ImplVulkan_RemoveTexture(set); });
		m_ImGuiTextures[index].Set = VK_NULL_HANDLE;
		m_FreeImGuiIds.push_back(index);
	}

	uint64_t VulkanContext::ResolveImGuiTexture(uint64_t id) const {
		if (id < kImGuiIdBase || id >= kImGuiIdBase + m_ImGuiTextures.size()) return id;
		const uint32_t index = static_cast<uint32_t>(id - kImGuiIdBase);
		auto* self = const_cast<VulkanContext*>(this);
		std::lock_guard<std::mutex> lock(s_ImGuiMutex);
		auto& entry = self->m_ImGuiTextures[index];
		if (!entry.Set && m_ImGuiReady) {
			auto it = s_Pending.find(index);
			if (it == s_Pending.end()) return 0;
			entry.Set = ImGui_ImplVulkan_AddTexture(it->second.Sampler, it->second.View, it->second.Layout);
			s_Pending.erase(it);
		}
		return reinterpret_cast<uint64_t>(entry.Set);
	}

	// ============================================================================
	// SHUTDOWN
	// ============================================================================

	void VulkanContext::Shutdown() {
		if (!m_Device) return;
		vkDeviceWaitIdle(m_Device);
		FlushDeletions(true);
		for (auto& f : m_Frames) {
			vkDestroyFence(m_Device, f.InFlight, nullptr);
			vkDestroySemaphore(m_Device, f.ImageAvailable, nullptr);
			vkDestroyCommandPool(m_Device, f.Pool, nullptr);
		}
		vkDestroyFence(m_Device, m_UploadFence, nullptr);
		vkDestroyCommandPool(m_Device, m_UploadPool, nullptr);
		for (auto& [k, s] : m_Samplers) vkDestroySampler(m_Device, s, nullptr);
		m_Samplers.clear();
		DestroySwapchain();
		// Resources still alive (static textures leaked on purpose) keep their memory: the
		// process is exiting and the driver reclaims it with the device
		for (auto& b : m_Blocks) vkFreeMemory(m_Device, b.Memory, nullptr);
		m_Blocks.clear();
		vkDestroyDevice(m_Device, nullptr);
		m_Device = VK_NULL_HANDLE;
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		if (m_Messenger) {
			auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (destroy) destroy(m_Instance, m_Messenger, nullptr);
		}
		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
	}

}
