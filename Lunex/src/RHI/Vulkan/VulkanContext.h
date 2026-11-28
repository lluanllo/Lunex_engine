#pragma once

/**
 * @file VulkanContext.h
 * @brief Vulkan backend core: instance, device, swapchain, frames in flight, memory.
 *
 * One context per process (the editor window). The rest of the Vulkan RHI (buffers,
 * textures, framebuffers, command list) allocates through it.
 *
 *  - Vulkan 1.3 core: dynamic rendering + synchronization2 (no VkRenderPass objects)
 *  - 2 frames in flight; per-frame command buffer, fence, semaphores and deferred deletions
 *  - Block sub-allocator per memory type (64 MB blocks): few vkAllocateMemory calls
 *  - Immediate submit for uploads (textures, static buffers) outside the frame
 *  - ImGui texture registry: engine 32-bit texture IDs -> VkDescriptorSet
 */

#include "Core/Core.h"

#include <vulkan/vulkan.h>

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace Lunex::RHI::Vk {

	constexpr uint32_t kFramesInFlight = 2;

	struct Allocation {
		VkDeviceMemory Memory = VK_NULL_HANDLE;
		VkDeviceSize Offset = 0;
		VkDeviceSize Size = 0;
		void* Mapped = nullptr;          // host-visible memory: pointer at Offset
		uint32_t MemoryType = 0;
		int32_t Block = -1;              // -1 = dedicated allocation
		bool IsValid() const { return Memory != VK_NULL_HANDLE; }
	};

	struct FrameData {
		VkCommandPool Pool = VK_NULL_HANDLE;
		VkCommandBuffer Cmd = VK_NULL_HANDLE;
		VkFence InFlight = VK_NULL_HANDLE;
		VkSemaphore ImageAvailable = VK_NULL_HANDLE;
	};

	class VulkanContext {
	public:
		static VulkanContext& Get();
		static bool IsActive();

		bool Init(GLFWwindow* window);
		void Shutdown();

		// ---- Frame ----
		// Waits for the frame slot, acquires the swapchain image, begins the command buffer.
		// Returns false when the frame must be skipped (minimized / swapchain recreated).
		bool BeginFrame();
		// Records `renderUI` into the swapchain image, submits and presents.
		void EndFrame(const std::function<void(VkCommandBuffer)>& renderUI);
		bool IsFrameActive() const { return m_FrameActive; }
		VkCommandBuffer GetCommandBuffer() const { return m_Frames[m_FrameIndex].Cmd; }
		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		uint64_t GetFrameNumber() const { return m_FrameNumber; }
		void OnResize(uint32_t width, uint32_t height);
		void SetVSync(bool enabled);
		bool IsVSync() const { return m_VSync; }

		// ---- Work outside the frame ----
		void ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record);
		// Destroy after the GPU is done with the current frame
		void DeferDelete(std::function<void()> fn);
		void WaitIdle();

		// ---- Memory ----
		uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
		Allocation Allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags props);
		void Free(const Allocation& alloc);
		uint64_t GetAllocatedBytes() const { return m_AllocatedBytes; }

		// ---- Samplers (cached) ----
		VkSampler GetSampler(VkFilter filter, VkSamplerMipmapMode mip, VkSamplerAddressMode address, bool compare = false, float anisotropy = 1.0f);

		// ---- ImGui textures: engine IDs (uint32) <-> descriptor sets ----
		uint32_t RegisterImGuiTexture(VkImageView view, VkSampler sampler, VkImageLayout layout);
		void UpdateImGuiTexture(uint32_t id, VkImageView view, VkSampler sampler, VkImageLayout layout);
		void UnregisterImGuiTexture(uint32_t id);
		// Returns the descriptor set for an engine ID, or `id` unchanged when it is not one of ours
		uint64_t ResolveImGuiTexture(uint64_t id) const;
		bool IsImGuiReady() const { return m_ImGuiReady; }
		void SetImGuiReady(bool ready) { m_ImGuiReady = ready; }

		// ---- Accessors ----
		VkInstance GetInstance() const { return m_Instance; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetDevice() const { return m_Device; }
		VkQueue GetQueue() const { return m_Queue; }
		uint32_t GetQueueFamily() const { return m_QueueFamily; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		VkFormat GetSwapchainFormat() const { return m_SwapchainFormat; }
		VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
		uint32_t GetSwapchainImageCount() const { return static_cast<uint32_t>(m_SwapchainImages.size()); }
		const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
		const std::string& GetDeviceName() const { return m_DeviceName; }
		bool HasDepthClipControl() const { return m_DepthClipControl; }

	private:
		bool CreateInstance();
		bool PickPhysicalDevice();
		bool CreateDevice();
		bool CreateSwapchain();
		void DestroySwapchain();
		void RecreateSwapchain();
		// Runs the deletions whose frame the GPU has finished (all of them with `all`)
		void FlushDeletions(bool all);

		GLFWwindow* m_Window = nullptr;
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_Messenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_Properties{};
		VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
		std::string m_DeviceName;
		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_Queue = VK_NULL_HANDLE;
		uint32_t m_QueueFamily = 0;
		bool m_DepthClipControl = false;
		bool m_Validation = false;

		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		VkFormat m_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
		VkExtent2D m_SwapchainExtent{ 0, 0 };
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainViews;
		std::vector<VkSemaphore> m_RenderFinished;     // one per swapchain image
		bool m_SwapchainDirty = false;
		bool m_VSync = true;

		FrameData m_Frames[kFramesInFlight];
		// Deferred deletions tagged with the frame number they were requested in: any thread may
		// add one, and one requested between frames (or in a skipped frame) still waits for every
		// frame that could reference the resource
		std::deque<std::pair<uint64_t, std::function<void()>>> m_Deletions;
		std::mutex m_DeletionMutex;
		uint32_t m_FrameIndex = 0;
		uint64_t m_FrameNumber = 0;
		uint32_t m_ImageIndex = 0;
		bool m_FrameActive = false;

		VkCommandPool m_UploadPool = VK_NULL_HANDLE;
		VkFence m_UploadFence = VK_NULL_HANDLE;
		std::mutex m_UploadMutex;

		// Block allocator
		struct Block {
			VkDeviceMemory Memory = VK_NULL_HANDLE;
			uint32_t MemoryType = 0;
			VkDeviceSize Size = 0;
			void* Mapped = nullptr;
			std::vector<std::pair<VkDeviceSize, VkDeviceSize>> Free;   // offset, size (sorted)
		};
		std::vector<Block> m_Blocks;
		uint64_t m_AllocatedBytes = 0;
		std::mutex m_AllocMutex;

		std::unordered_map<uint64_t, VkSampler> m_Samplers;

		struct ImGuiTexture { VkDescriptorSet Set = VK_NULL_HANDLE; };
		std::vector<ImGuiTexture> m_ImGuiTextures;     // index = engine ID - kImGuiIdBase
		std::vector<uint32_t> m_FreeImGuiIds;
		bool m_ImGuiReady = false;
	};

	// Engine texture IDs handed to ImGui start here (never collide with GL names in practice)
	constexpr uint32_t kImGuiIdBase = 0x40000000u;

	const char* ResultString(VkResult r);
	#define LNX_VK_CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) LNX_LOG_ERROR("Vulkan: {0} failed ({1})", #x, ::Lunex::RHI::Vk::ResultString(_r)); } while (0)

}
