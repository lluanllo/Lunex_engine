#include "stpch.h"

#include "GraphicsContext.h"

#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHIContext.h"
#include "RHI/Vulkan/VulkanRHIContext.h"

#include <GLFW/glfw3.h>

namespace Lunex {
	
	// ============================================================================
	// OPENGL GRAPHICS CONTEXT ADAPTER
	// ============================================================================
	
	class OpenGLGraphicsContextAdapter : public GraphicsContext {
	public:
		OpenGLGraphicsContextAdapter(void* window)
			: m_Window(window)
		{
			m_RHIContext = CreateScope<RHI::OpenGLRHIContext>(window);
		}
		
		virtual ~OpenGLGraphicsContextAdapter() = default;
		
		void Init() override {
			if (m_RHIContext) {
				m_RHIContext->Initialize();
			}
		}
		
		void SwapBuffers() override {
			if (m_Swapchain) {
				m_Swapchain->Present();
			} else if (m_RHIContext) {
				RHI::SwapchainCreateInfo info;
				info.Width = 1280;
				info.Height = 720;
				info.VSync = true;
				m_Swapchain = m_RHIContext->CreateSwapchain(info);
				if (m_Swapchain) {
					m_Swapchain->Present();
				}
			}
		}
		
	private:
		void* m_Window = nullptr;
		Scope<RHI::OpenGLRHIContext> m_RHIContext;
		Ref<RHI::RHISwapchain> m_Swapchain;
	};

	// ============================================================================
	// VULKAN GRAPHICS CONTEXT ADAPTER
	// ============================================================================
	
	class VulkanGraphicsContextAdapter : public GraphicsContext {
	public:
		VulkanGraphicsContextAdapter(void* window)
			: m_Window(window)
		{
		}
		
		virtual ~VulkanGraphicsContextAdapter() = default;
		
		void Init() override {
			// Vulkan context initialization is handled by RHI::Initialize
			// Nothing to do here - the RHI layer manages the Vulkan instance/device
		}
		
		void SwapBuffers() override {
			// Vulkan presentation is handled by the swapchain in the RHI layer
			// For now, do nothing - the engine's render loop handles present via RHI
			if (m_Swapchain) {
				m_Swapchain->Present();
			}
		}

		void SetSwapchain(Ref<RHI::RHISwapchain> swapchain) { m_Swapchain = swapchain; }
		
	private:
		void* m_Window = nullptr;
		Ref<RHI::RHISwapchain> m_Swapchain;
	};
	
	Scope<GraphicsContext> GraphicsContext::Create(void* window, RHI::GraphicsAPI api) {
		switch (api) {
			case RHI::GraphicsAPI::OpenGL:
				return CreateScope<OpenGLGraphicsContextAdapter>(window);
			case RHI::GraphicsAPI::Vulkan:
				return CreateScope<VulkanGraphicsContextAdapter>(window);
			default:
				return CreateScope<OpenGLGraphicsContextAdapter>(window);
		}
	}
}