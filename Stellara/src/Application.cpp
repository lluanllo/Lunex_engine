#include "stpch.h"

#include "Events/ApplicationEvent.h"
#include "Application.h"

#include <glad/glad.h>

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

	void Stellara::Application::PushLayer(Layer* layer) {
		m_LayerStack.PushLayer(layer);
	}

	void Stellara::Application::PushOverlay(Layer* overlay) {
		m_LayerStack.PushOverlay(overlay);
	}

	void Stellara::Application::OnEvent(Event& e) {

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));

		STLR_LOG_TRACE("Event: {0}", e.ToString());
		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			(*--it)->OnEvent(e);
			if (e.m_Handled) {
				break;
			}
		}
	}

	void Stellara::Application::Run() {
		while (m_Running){
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			for (Layer* layer : m_LayerStack) {
				layer->OnUpdate();
			}

			m_Window->OnUpdate();
		}
	}

	bool Stellara::Application::OnWindowClose(WindowCloseEvent& e) {
		m_Running = false;
		return true;
	}
}