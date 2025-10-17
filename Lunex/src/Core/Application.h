#pragma once

#include "Core/Core.h"

#include "Window.h"
#include "LayerStack.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

#include "Core/Timestep.h"

#include "ImGui/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Lunex {
	struct ApplicationCommandLineArgs {
		int Count = 0;
		char** Args = nullptr;
		
		const char* operator[](int index) const	{
			LNX_CORE_ASSERT(index < Count);
			return Args[index];
		}
	};
	
	class Application {
		public:
			Application(const std::string& name = "Lunex App", ApplicationCommandLineArgs args = ApplicationCommandLineArgs());
			virtual ~Application();
			
			void OnEvent(Event& e);
			
			void PushLayer(Layer* layer);
			void PushOverlay(Layer* layer);
			
			Window& GetWindow() { return *m_Window; }
			
			void Close();
			
			ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
			
			static Application& Get() { return *s_Instance; }
			
			ApplicationCommandLineArgs GetCommandLineArgs() const { return m_CommandLineArgs; }
			
		private:
			void Run();
			bool OnWindowClose(WindowCloseEvent& e);
			bool OnWindowResize(WindowResizeEvent& e);
			
		private:
			ApplicationCommandLineArgs m_CommandLineArgs;
			std::unique_ptr<Window> m_Window;
			ImGuiLayer* m_ImGuiLayer;
			
			bool m_Running = true;
			bool m_Minimized = false;
			
			LayerStack m_LayerStack;
			
			float m_LastFrameTime = 0.0f;
			
		private:
			static Application* s_Instance;
			friend int ::main(int argc, char** argv);
			
	};
	
	// To be defined in CLIENT
	Ref<Application> CreateApplication(ApplicationCommandLineArgs args);
}
