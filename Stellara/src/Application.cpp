#include "stpch.h"

#include "Events/ApplicationEvent.h"
#include "Core/Stellara.hpp"

#include <GLFW/glfw3.h>

namespace Stellara{

	Stellara::Application::Application() {
		Stellara::Log::Init();
		STLR_LOG_INFO("Logger Initialized");
		STLR_LOG_DEBUG("Stellara Application Initialized");

		m_Window = std::unique_ptr<Window>(Window::Create());
	}

	Stellara::Application::~Application() {
		// Cleanup code here
	}

	void Stellara::Application::Run() {
		while (true){
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			m_Window->OnUpdate();
		}
	}

}