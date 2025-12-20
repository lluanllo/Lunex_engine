#include "stpch.h"
#include "OpenGLRHIContext.h"
#include "Log/Log.h"

#include <GLFW/glfw3.h>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL SWAPCHAIN IMPLEMENTATION
	// ============================================================================
	
	OpenGLSwapchain::OpenGLSwapchain(GLFWwindow* window, const SwapchainCreateInfo& info)
		: m_Window(window)
		, m_Width(info.Width)
		, m_Height(info.Height)
		, m_Format(info.Format)
		, m_VSync(info.VSync)
	{
		SetVSync(m_VSync);
	}
	
	OpenGLSwapchain::~OpenGLSwapchain() {
		// Nothing to destroy - OpenGL uses default framebuffer
	}
	
	uint32_t OpenGLSwapchain::AcquireNextImage() {
		// OpenGL doesn't need explicit acquisition
		m_CurrentBuffer = (m_CurrentBuffer + 1) % 2;
		return m_CurrentBuffer;
	}
	
	void OpenGLSwapchain::Present() {
		glfwSwapBuffers(m_Window);
	}
	
	void OpenGLSwapchain::Resize(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;
		glViewport(0, 0, width, height);
	}
	
	Ref<RHITexture2D> OpenGLSwapchain::GetBackbuffer(uint32_t index) const {
		// OpenGL doesn't expose backbuffer as texture directly
		// For now, return nullptr - advanced usage would require FBO copy
		return nullptr;
	}
	
	Ref<RHIFramebuffer> OpenGLSwapchain::GetCurrentFramebuffer() const {
		// Default framebuffer (0)
		return nullptr;
	}
	
	void OpenGLSwapchain::SetVSync(bool enabled) {
		m_VSync = enabled;
		glfwSwapInterval(enabled ? 1 : 0);
		m_PresentMode = enabled ? PresentMode::VSync : PresentMode::Immediate;
	}
	
	void OpenGLSwapchain::SetPresentMode(PresentMode mode) {
		m_PresentMode = mode;
		switch (mode) {
			case PresentMode::Immediate:
				glfwSwapInterval(0);
				m_VSync = false;
				break;
			case PresentMode::VSync:
			case PresentMode::FIFO:
				glfwSwapInterval(1);
				m_VSync = true;
				break;
			case PresentMode::Mailbox:
				// OpenGL doesn't have true mailbox, use adaptive vsync if available
				glfwSwapInterval(-1);  // Adaptive vsync (EXT_swap_control_tear)
				m_VSync = true;
				break;
		}
	}

	// ============================================================================
	// OPENGL RHI CONTEXT IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIContext::OpenGLRHIContext(void* windowHandle)
		: m_Window(static_cast<GLFWwindow*>(windowHandle))
	{
	}
	
	OpenGLRHIContext::~OpenGLRHIContext() {
		Shutdown();
	}
	
	bool OpenGLRHIContext::Initialize() {
		if (m_Initialized) {
			return true;
		}
		
		// Check if there's already a current context (created by Application)
		GLFWwindow* currentContext = glfwGetCurrentContext();
		
		if (!m_Window && currentContext) {
			// Use the existing context from Application
			m_Window = currentContext;
			LNX_LOG_INFO("OpenGLRHIContext: Using existing OpenGL context");
		}
		else if (!m_Window) {
			LNX_LOG_ERROR("OpenGLRHIContext: No window handle provided and no existing context!");
			return false;
		}
		else {
			// Make our window current
			glfwMakeContextCurrent(m_Window);
		}
		
		// Check if GLAD is already initialized
		if (glGetString == nullptr) {
			// Load OpenGL functions
			int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
			if (!status) {
				LNX_LOG_ERROR("OpenGLRHIContext: Failed to initialize GLAD!");
				return false;
			}
		}
		
		// Get version info
		m_VersionMajor = GLVersion.major;
		m_VersionMinor = GLVersion.minor;
		m_VersionString = std::string((const char*)glGetString(GL_VERSION));
		
		LNX_LOG_INFO("OpenGL RHI Context Initialized");
		LNX_LOG_INFO("  Version: {0}", m_VersionString);
		LNX_LOG_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		LNX_LOG_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		LNX_LOG_INFO("  GLSL Version: {0}", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
		
		// Enable debug output in debug builds
		#ifdef LNX_DEBUG
			EnableDebugOutput(true);
		#endif
		
		m_Initialized = true;
		s_Instance = this;
		
		return true;
	}
	
	void OpenGLRHIContext::Shutdown() {
		if (!m_Initialized) {
			return;
		}
		
		if (s_Instance == this) {
			s_Instance = nullptr;
		}
		
		m_Initialized = false;
		LNX_LOG_INFO("OpenGL RHI Context shutdown");
	}
	
	void OpenGLRHIContext::MakeCurrent() {
		if (m_Window) {
			glfwMakeContextCurrent(m_Window);
		}
	}
	
	Ref<RHISwapchain> OpenGLRHIContext::CreateSwapchain(const SwapchainCreateInfo& info) {
		return CreateRef<OpenGLSwapchain>(m_Window, info);
	}
	
	std::string OpenGLRHIContext::GetAPIVersion() const {
		return m_VersionString;
	}
	
	void OpenGLRHIContext::EnableDebugOutput(bool enable) {
		m_DebugEnabled = enable;
		
		if (enable) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(DebugCallback, nullptr);
			
			// Disable notification-level messages
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 
								  0, nullptr, GL_FALSE);
			
			LNX_LOG_INFO("OpenGL debug output enabled");
		} else {
			glDisable(GL_DEBUG_OUTPUT);
		}
	}
	
	void OpenGLRHIContext::PushDebugGroup(const std::string& name) {
		if (m_DebugEnabled && GLAD_GL_KHR_debug) {
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
		}
	}
	
	void OpenGLRHIContext::PopDebugGroup() {
		if (m_DebugEnabled && GLAD_GL_KHR_debug) {
			glPopDebugGroup();
		}
	}
	
	void OpenGLRHIContext::InsertDebugMarker(const std::string& name) {
		if (m_DebugEnabled && GLAD_GL_KHR_debug) {
			glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
								 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, name.c_str());
		}
	}
	
	void GLAPIENTRY OpenGLRHIContext::DebugCallback(
		GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void* userParam)
	{
		// Ignore certain verbose messages
		if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
			return;
		}
		
		// Determine source string
		const char* sourceStr = "Unknown";
		switch (source) {
			case GL_DEBUG_SOURCE_API: sourceStr = "API"; break;
			case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceStr = "Window System"; break;
			case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
			case GL_DEBUG_SOURCE_THIRD_PARTY: sourceStr = "Third Party"; break;
			case GL_DEBUG_SOURCE_APPLICATION: sourceStr = "Application"; break;
			case GL_DEBUG_SOURCE_OTHER: sourceStr = "Other"; break;
		}
		
		// Determine type string
		const char* typeStr = "Unknown";
		switch (type) {
			case GL_DEBUG_TYPE_ERROR: typeStr = "Error"; break;
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeStr = "Undefined Behavior"; break;
			case GL_DEBUG_TYPE_PORTABILITY: typeStr = "Portability"; break;
			case GL_DEBUG_TYPE_PERFORMANCE: typeStr = "Performance"; break;
			case GL_DEBUG_TYPE_MARKER: typeStr = "Marker"; break;
			case GL_DEBUG_TYPE_OTHER: typeStr = "Other"; break;
		}
		
		// Log based on severity
		switch (severity) {
			case GL_DEBUG_SEVERITY_HIGH:
				LNX_LOG_ERROR("[OpenGL {0}] {1} ({2}): {3}", sourceStr, typeStr, id, message);
				break;
			case GL_DEBUG_SEVERITY_MEDIUM:
				LNX_LOG_WARN("[OpenGL {0}] {1} ({2}): {3}", sourceStr, typeStr, id, message);
				break;
			case GL_DEBUG_SEVERITY_LOW:
				LNX_LOG_INFO("[OpenGL {0}] {1} ({2}): {3}", sourceStr, typeStr, id, message);
				break;
			case GL_DEBUG_SEVERITY_NOTIFICATION:
				LNX_LOG_TRACE("[OpenGL {0}] {1} ({2}): {3}", sourceStr, typeStr, id, message);
				break;
		}
	}

	// ============================================================================
	// FACTORY IMPLEMENTATIONS
	// ============================================================================
	
	Scope<RHIContext> RHIContext::Create(GraphicsAPI api, void* windowHandle) {
		switch (api) {
			case GraphicsAPI::OpenGL:
				return CreateScope<OpenGLRHIContext>(windowHandle);
			default:
				LNX_LOG_ERROR("RHIContext::Create: Unsupported graphics API!");
				return nullptr;
		}
	}

} // namespace RHI
} // namespace Lunex
