#include "stpch.h"

#include "Events/ApplicationEvent.h"
#include "Application.h"

#include "Renderer/Renderer.h"

#include "Input.h"

namespace Lunex{
	
	#define BIND_EVENT_FN(fn) std::bind(&Application::fn, this, std::placeholders::_1)
	
	Application* Application::s_Instance = nullptr;
	
	Application::Application() {
		
		LNX_PROFILE_FUNCTION();
		
		LN_CORE_ASSERT(s_Instance == nullptr, "Application already exists!");
		s_Instance = this;
		
		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
		
		Renderer::Init();
		
		m_ImGuiLayer = new ImGuiLayer;
		PushOverlay(m_ImGuiLayer);
	}
	
	Application::~Application() {
		LNX_PROFILE_FUNCTION();
		Renderer::Shutdown();
	}
	
	void Application::PushLayer(Layer* layer) {
		LNX_PROFILE_FUNCTION();
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}
	
	void Application::PushOverlay(Layer* overlay) {
		LNX_PROFILE_FUNCTION();
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}
	
	void Application::Close() {
		m_Running = false;
	}
	
	void Application::OnEvent(Event& e) {
		
		LNX_PROFILE_FUNCTION();
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
		
		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			(*--it)->OnEvent(e);
			if (e.m_Handled) {
				break;
			}
		}
	}
	
	void Application::Run() {
		LNX_PROFILE_FUNCTION();
		while (m_Running) {	
			
			LNX_PROFILE_SCOPE("RunLoop");
			
			float time = (float)glfwGetTime();
			Timestep timstep = time - m_LastFrameTime;
			m_LastFrameTime = time;
			
			if (!m_Minimized) {
				{
					LNX_PROFILE_SCOPE("LayerStack OnUpdate");
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timstep);
				}
			}
			
			m_ImGuiLayer->Begin();
			{
				LNX_PROFILE_SCOPE("LayerStack OnImGuiRender");
				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();				
			}
			m_ImGuiLayer->End();
			
			m_Window->OnUpdate();
		}
	}
	
	bool Application::OnWindowClose(WindowCloseEvent& e) {
		m_Running = false;
		return true;
	}
	
	bool Application::OnWindowResize(WindowResizeEvent& e) {
		LNX_PROFILE_FUNCTION();
		if (e.GetWidth() == 0 || e.GetHeight() == 0) {
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		Renderer::onWindowResize(e.GetWidth(), e.GetHeight());
		
		return false;
	}
}