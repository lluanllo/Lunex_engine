#pragma once

#include "Core/Core.h"

#include "Window.h"
#include "LayerStack.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

#include "Core/Timestep.h"

#include "ImGui/ImGuiLayer.h"

#include "../../vendor/GLFW/include/GLFW/glfw3.h"

namespace Lunex {
	class LUNEX_API Application {
		public:
			Application(const std::string& name = "Lunex App");
			virtual ~Application();
			
			void Run();
			
			void OnEvent(Event& e);
			
			void PushLayer(Layer* layer);
			void PushOverlay(Layer* layer);
			
			inline static Application& Get() { return *s_Instance; }
			inline Window& GetWindow() { return *m_Window; }
			
			void Close();
			
			static Application& GetApplication() { return *s_Instance; }
			
			
		private:
			bool OnWindowClose(WindowCloseEvent& e);
			bool OnWindowResize(WindowResizeEvent& e);
			
		private:
			std::unique_ptr<Window> m_Window;
			ImGuiLayer* m_ImGuiLayer;
			
			bool m_Running = true;
			bool m_Minimized = false;
			
			LayerStack m_LayerStack;
			
			float m_LastFrameTime = 0.0f;
			Timestep m_Timestep;
			
		private:
			static Application* s_Instance;
			
	};

	// To be defined in CLIENT
	Ref<Application> CreateApplication();
}
