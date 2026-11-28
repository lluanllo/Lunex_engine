#pragma once

/**
 * @file VulkanDevice.h
 * @brief RHI device + context for the Vulkan backend.
 *
 * Migration strategy: resource types already implemented in Vulkan are created natively;
 * the rest inherit the OpenGL implementations, which run against no-op GL entry points
 * (LoadOpenGLStubs) while the Vulkan backend is active. Subsystems are ported one by one
 * without the editor ever crashing on a GL call.
 */

#include "RHI/RHIContext.h"
#include "RHI/OpenGL/OpenGLRHIDevice.h"

struct GLFWwindow;

namespace Lunex::RHI::Vk {

	// Every gl* entry point becomes a harmless no-op (glGetString returns a fake version string,
	// glMap* returns scratch memory). Called instead of gladLoadGLLoader when Vulkan is active.
	void LoadOpenGLStubs();

	class VulkanRHIDevice : public OpenGLRHIDevice {
	public:
		VulkanRHIDevice();
		GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
		const DeviceCapabilities& GetCapabilities() const override { return m_VkCaps; }
		const std::string& GetDeviceName() const override { return m_VkName; }

		Ref<RHITexture2D> CreateTexture2D(const TextureCreateInfo& info) override;
		uint64_t GetAllocatedMemory() const override;
		void WaitIdle() override;

	private:
		DeviceCapabilities m_VkCaps;
		std::string m_VkName;
	};

	class VulkanRHIContext : public RHIContext {
	public:
		explicit VulkanRHIContext(void* window) : m_Window(static_cast<GLFWwindow*>(window)) {}
		bool Initialize() override;
		void Shutdown() override;
		void MakeCurrent() override {}
		Ref<RHISwapchain> CreateSwapchain(const SwapchainCreateInfo&) override { return nullptr; }
		GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
		std::string GetAPIVersion() const override;
		bool IsValid() const override;
		void EnableDebugOutput(bool) override {}
		void PushDebugGroup(const std::string&) override {}
		void PopDebugGroup() override {}
		void InsertDebugMarker(const std::string&) override {}

	private:
		GLFWwindow* m_Window = nullptr;
	};

}
