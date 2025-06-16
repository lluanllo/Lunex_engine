#include "stpch.h"

#include "Events/ApplicationEvent.h"
#include "Core/Stellara.hpp"

#include <GLFW/glfw3.h>

namespace Stellara{

	#define BIND_EVENT_FN(fn) std::bind(&Application::fn, this, std::placeholders::_1)

	Stellara::Application::Application() {
		Stellara::Log::Init();
		STLR_LOG_INFO("Logger Initialized");
		STLR_LOG_DEBUG("Stellara Application Initialized");

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
	}

	Stellara::Application::~Application() {
		// Cleanup code here
	}

	void Stellara::Application::OnEvent(Event& e) {

	}

	void Stellara::Application::Run() {
		while (true){
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			m_Window->OnUpdate();
		}
	}

}