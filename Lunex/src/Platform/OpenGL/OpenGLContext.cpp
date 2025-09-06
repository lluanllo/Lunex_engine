#include "stpch.h"

#include "OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <GL/GL.h>

namespace Lunex {

	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		LN_CORE_ASSERT(windowHandle, "Window handle is null!")
	}

	void OpenGLContext::Init(){
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		LN_CORE_ASSERT(status, "Failed to initialize Glad!");

		LNX_LOG_INFO("OpenGL Info:");
		//STLR_LOG_INFO("OpenGL Vendor: {0}", glGetString(GL_VENDOR));
		//STLR_LOG_INFO("OpenGL Renderer: {0}", glGetString(GL_RENDERER));
		//STLR_LOG_INFO("OpenGL Version: {0}", glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers(){
		glfwSwapBuffers(m_WindowHandle);
	}
}