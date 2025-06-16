#pragma once

#include "Events/Event.h"
#include "Events/ApplicationEvent.h"
#include "Window.h"

namespace Stellara {

	class Application {
		public:
			Application();
			virtual ~Application();

			void Run();

			void OnEvent(Event& e);
		private:
			bool OnWindowClose(WindowCloseEvent& e);

			std::unique_ptr<Window> m_Window;
			bool m_Running = true;
	};
}
