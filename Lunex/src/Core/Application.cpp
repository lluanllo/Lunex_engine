#include "stpch.h"
#include "Application.h"

#include "Renderer/Renderer.h"
#include "Renderer/MaterialRegistry.h"

#include "Input.h"

#include <GLFW/glfw3.h>
#include <filesystem>

namespace Lunex{	
	Application* Application::s_Instance = nullptr;
	
	Application::Application(const std::string& name, ApplicationCommandLineArgs args)
		: m_CommandLineArgs(args)
	{
		
		LNX_PROFILE_FUNCTION();
		
		LNX_CORE_ASSERT(s_Instance == nullptr, "Application already exists!");
		s_Instance = this;
		
		// ========================================
		// CREATE WINDOW FIRST (will create OpenGL context via GLFW)
		// ========================================
		// WindowsWindow::Init creates GLFW window and OpenGL context
		// GraphicsContext wraps the existing context
		m_Window = Window::Create(WindowProps(name));
		m_Window->SetEventCallback(LNX_BIND_EVENT_FN(Application::OnEvent));
		
		// ========================================
		// INITIALIZE RHI (uses existing OpenGL context from window)
		// ========================================
		// Pass the GLFW window handle so RHI can use the existing context
		auto* glfwWindow = static_cast<GLFWwindow*>(m_Window->GetNativeWindow());
		if (!RHI::Initialize(RHI::GraphicsAPI::OpenGL, glfwWindow)) {
			LNX_LOG_ERROR("Failed to initialize RHI!");
		}
		
		// Complete renderer initialization
		Renderer::Init();
		
		// ========================================
		// Initialize Material System
		// ========================================
		// MaterialRegistry is a singleton that manages all material assets
		// It creates the default material on first access
		MaterialRegistry::Get(); // Initialize registry and create default material
		LNX_LOG_INFO("? Material System initialized");
		
		m_ImGuiLayer = new ImGuiLayer;
		PushOverlay(m_ImGuiLayer);
	}
	
	Application::~Application() {
		LNX_PROFILE_FUNCTION();
		
		// ========================================
		// Shutdown Material System
		// ========================================
		MaterialRegistry::Get().ClearAll();
		LNX_LOG_INFO("Material System shutdown");
		
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
		dispatcher.Dispatch<WindowCloseEvent>(LNX_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(LNX_BIND_EVENT_FN(Application::OnWindowResize));
		
		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			--it;
			if (e.m_Handled)
				break;
			(*it)->OnEvent(e);
		}
	}
	
	void Application::Run() {
		LNX_PROFILE_FUNCTION();
		while (m_Running) {	
			
			LNX_PROFILE_SCOPE("RunLoop");
			
			float time = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;
			
			if (!m_Minimized) {
				{
					LNX_PROFILE_SCOPE("LayerStack OnUpdate");
					
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}
				
				m_ImGuiLayer->Begin();
				{
					LNX_PROFILE_SCOPE("LayerStack OnImGuiRender");
					
					for (Layer* layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}
			
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