#pragma once

#include "Core/Core.h"

#include "Window.h"
#include "LayerStack.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"

#include "ImGui/ImGuiLayer.h"

namespace Stellara {

	class STELLARA_API Application {
		public:
			Application();
			virtual ~Application();

			void Run();

			void OnEvent(Event& e);

			void PushLayer(Layer* layer);
			void PushOverlay(Layer* layer);

			inline static Application& Get() { return *s_Instance; }
			inline Window& GetWindow() { return *m_Window; }

		private:
			bool OnWindowClose(WindowCloseEvent& e);

			std::unique_ptr<Window> m_Window;
			ImGuiLayer* m_ImGuiLayer;

			bool m_Running = true;
			LayerStack m_LayerStack;

			unsigned int m_vertexArray,	m_indexBuffer;
			std::unique_ptr<Shader> m_Shader;
			std::unique_ptr<VertexBuffer> m_VertexBuffer;

		private:
			static Application* s_Instance;
		
	};

	// To be defined in CLIENT
	Application* CreateApplication();
}
