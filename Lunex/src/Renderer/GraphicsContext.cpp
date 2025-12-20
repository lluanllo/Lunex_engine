#include "stpch.h"

#include "GraphicsContext.h"

#include "Renderer.h"
#include "RHI/OpenGL/OpenGLRHIContext.h"

namespace Lunex {
	
	// ============================================================================
	// GRAPHICS CONTEXT ADAPTER - Wraps RHIContext for legacy API compatibility
	// ============================================================================
	
	class GraphicsContextAdapter : public GraphicsContext {
	public:
		GraphicsContextAdapter(void* window)
			: m_Window(window)
		{
			m_RHIContext = CreateScope<RHI::OpenGLRHIContext>(window);
		}
		
		virtual ~GraphicsContextAdapter() = default;
		
		void Init() override {
			if (m_RHIContext) {
				m_RHIContext->Initialize();
			}
		}
		
		void SwapBuffers() override {
			if (m_Swapchain) {
				m_Swapchain->Present();
			} else if (m_RHIContext) {
				// Create default swapchain on first use
				RHI::SwapchainCreateInfo info;
				info.Width = 1280;  // Will be resized by window
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
	
	Scope<GraphicsContext> GraphicsContext::Create(void* window) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				return CreateScope<GraphicsContextAdapter>(window);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}