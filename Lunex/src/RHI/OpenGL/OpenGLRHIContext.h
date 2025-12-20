#pragma once

/**
 * @file OpenGLRHIContext.h
 * @brief OpenGL implementation of RHIContext
 */

#include "RHI/RHIContext.h"
#include <glad/glad.h>

// Fallback for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

struct GLFWwindow;

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL SWAPCHAIN
	// ============================================================================
	
	class OpenGLSwapchain : public RHISwapchain {
	public:
		OpenGLSwapchain(GLFWwindow* window, const SwapchainCreateInfo& info);
		virtual ~OpenGLSwapchain();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return 0; }  // Default framebuffer
		bool IsValid() const override { return m_Window != nullptr; }
		
		// Swapchain operations
		uint32_t AcquireNextImage() override;
		void Present() override;
		void Resize(uint32_t width, uint32_t height) override;
		
		// Properties
		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		TextureFormat GetFormat() const override { return m_Format; }
		uint32_t GetBufferCount() const override { return 2; }  // Double buffered
		uint32_t GetCurrentBufferIndex() const override { return m_CurrentBuffer; }
		
		Ref<RHITexture2D> GetBackbuffer(uint32_t index) const override;
		Ref<RHIFramebuffer> GetCurrentFramebuffer() const override;
		
		// VSync
		void SetVSync(bool enabled) override;
		bool IsVSyncEnabled() const override { return m_VSync; }
		void SetPresentMode(PresentMode mode) override;
		PresentMode GetPresentMode() const override { return m_PresentMode; }
		
	private:
		GLFWwindow* m_Window = nullptr;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		TextureFormat m_Format = TextureFormat::RGBA8;
		bool m_VSync = true;
		PresentMode m_PresentMode = PresentMode::VSync;
		uint32_t m_CurrentBuffer = 0;
	};

	// ============================================================================
	// OPENGL RHI CONTEXT
	// ============================================================================
	
	class OpenGLRHIContext : public RHIContext {
	public:
		OpenGLRHIContext(void* windowHandle);
		virtual ~OpenGLRHIContext();
		
		// Lifecycle
		bool Initialize() override;
		void Shutdown() override;
		void MakeCurrent() override;
		
		// Swapchain
		Ref<RHISwapchain> CreateSwapchain(const SwapchainCreateInfo& info) override;
		
		// Info
		GraphicsAPI GetAPI() const override { return GraphicsAPI::OpenGL; }
		std::string GetAPIVersion() const override;
		bool IsValid() const override { return m_Initialized; }
		
		// Debug
		void EnableDebugOutput(bool enable) override;
		void PushDebugGroup(const std::string& name) override;
		void PopDebugGroup() override;
		void InsertDebugMarker(const std::string& name) override;
		
		// OpenGL specific
		GLFWwindow* GetWindow() const { return m_Window; }
		
	private:
		static void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
											  GLenum severity, GLsizei length,
											  const GLchar* message, const void* userParam);
		
		GLFWwindow* m_Window = nullptr;
		bool m_Initialized = false;
		bool m_DebugEnabled = false;
		
		// OpenGL version info
		int m_VersionMajor = 0;
		int m_VersionMinor = 0;
		std::string m_VersionString;
	};

} // namespace RHI
} // namespace Lunex
