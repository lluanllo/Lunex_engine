#include "stpch.h"
#include "WindowsWindow.h"
#include "Renderer/GraphicsContext.h"

#include "Events/ApplicationEvent.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/FileDropEvent.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Lunex {

	static uint8_t s_GLFWInitialized = 0;
	
	static void GLFWErrorCallback(int error, const char* description) {
		LNX_LOG_ERROR("GLFW Error ({0}): {1}", error, description);
	}
	
	WindowsWindow::WindowsWindow(const WindowProps& props) {
		LNX_PROFILE_FUNCTION();
		Init(props);
	}
	
	WindowsWindow::~WindowsWindow() {
		LNX_PROFILE_FUNCTION();
		Shutdown();
	}
	
	void WindowsWindow::Init(const WindowProps& props) {
		LNX_PROFILE_FUNCTION();
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;
		
		LNX_LOG_INFO("Creating window: {0} ({1}, {2})", m_Data.Title, m_Data.Width, m_Data.Height);
		
		if (s_GLFWInitialized == 0) {
			LNX_PROFILE_SCOPE("glfwInit");
			int success = glfwInit();
			LNX_CORE_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}
		{
			LNX_PROFILE_SCOPE("glfwCreateWindow");
			m_Window = glfwCreateWindow((int)m_Data.Width, (int)m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);
			++s_GLFWInitialized;
			LNX_CORE_ASSERT(m_Window, "Could not create GLFW window!");
		}
		
		// Set window icon
		LoadWindowIcon("Lunex-Editor/Resources/Icons/LunexLogo/LunexLogo.png");
		
		m_Context = GraphicsContext::Create(m_Window);
		m_Context->Init();
		
		glfwSetWindowUserPointer(m_Window, &m_Data);
		SetVSync(false);
		
		//Set GLFW callbacks
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			
			data.Width = width;
			data.Height = height;
			
			WindowResizeEvent event(width, height);
			data.EventCallback(event);
			LNX_LOG_WARN("Window resized: {0}, {1}", width, height);
		});
		
		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
			LNX_LOG_INFO("Window closed");
		});
		
		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action) {
				case GLFW_PRESS: {
					KeyPressedEvent event(static_cast<KeyCode>(key), 0);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(static_cast<KeyCode>(key));
					data.EventCallback(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(static_cast<KeyCode>(key), 1);
					data.EventCallback(event);
					break;
				}
			}
		});
		
		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode){
			
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			
			KeyTypedEvent event(static_cast<KeyCode>(keycode));
			data.EventCallback(event);
		});
		
		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action) {
				case GLFW_PRESS: {
					MouseButtonPressedEvent event(static_cast<MouseCode>(button));
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE: {
					MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
					data.EventCallback(event);
					break;
				}
			}
		});
		
		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
		});
		
		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});
		
		// File drop callback for drag & drop from Windows Explorer
		glfwSetDropCallback(m_Window, [](GLFWwindow* window, int count, const char** paths) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			
			std::vector<std::string> files;
			for (int i = 0; i < count; i++) {
				files.push_back(std::string(paths[i]));
			}
			
			FileDropEvent event(files);
			data.EventCallback(event);
		});
	}
	
	void WindowsWindow::Shutdown() {
		LNX_PROFILE_FUNCTION();
		glfwDestroyWindow(m_Window);
	}
	
	void WindowsWindow::OnUpdate() {
		LNX_PROFILE_FUNCTION();
		glfwPollEvents();
		m_Context->SwapBuffers();
	}
	
	void WindowsWindow::SetVSync(bool enabled) {
		LNX_PROFILE_FUNCTION();
		
		if (enabled) {
			glfwSwapInterval(1);
		}
		else {
			glfwSwapInterval(0);
		}
		m_Data.VSync = enabled;
		
		LNX_LOG_INFO("VSync is now {0}", enabled ? "enabled" : "enabled");
	}
	
	bool WindowsWindow::IsVSync() const {
		return m_Data.VSync;
	}

	void WindowsWindow::SetWindowIcon(const std::string& iconPath) {
		LoadWindowIcon(iconPath);
	}

	void WindowsWindow::LoadWindowIcon(const std::string& iconPath) {
		int width, height, channels;
		stbi_set_flip_vertically_on_load(0); // Don't flip for window icons
		
		// Try to load the image
		unsigned char* data = stbi_load(iconPath.c_str(), &width, &height, &channels, 4); // Force RGBA
		
		if (data) {
			GLFWimage icon;
			icon.width = width;
			icon.height = height;
			icon.pixels = data;
			
			// GLFW usará el tamaño nativo de la imagen
			// Para mejor resultado, asegúrate de que LunexLogo.png sea al menos 256x256 píxeles
			glfwSetWindowIcon(m_Window, 1, &icon);
			
			stbi_image_free(data);
			LNX_LOG_INFO("Window icon set successfully: {0} (Size: {1}x{2}, Channels: {3})", 
				iconPath, width, height, channels);
		}
		else {
			// Log error with stbi reason
			const char* error = stbi_failure_reason();
			LNX_LOG_ERROR("Failed to load window icon: {0} - Reason: {1}", iconPath, error);
			
			// Try alternative path
			std::string altPath = "Resources/Icons/LunexLogo/LunexLogo.png";
			LNX_LOG_INFO("Trying alternative path: {0}", altPath);
			
			data = stbi_load(altPath.c_str(), &width, &height, &channels, 4);
			if (data) {
				GLFWimage icon;
				icon.width = width;
				icon.height = height;
				icon.pixels = data;
				
				glfwSetWindowIcon(m_Window, 1, &icon);
				stbi_image_free(data);
				LNX_LOG_INFO("Window icon set successfully with alternative path: {0} ({1}x{2})", 
					altPath, width, height);
			}
			else {
				LNX_LOG_ERROR("Failed to load window icon from alternative path as well");
			}
		}
		
		stbi_set_flip_vertically_on_load(1); // Reset to default
	}
}