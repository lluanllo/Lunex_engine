#pragma once

#include "Events/Event.h"
#include "Window.h"

namespace Stellara {

	class Application {
		public:
			Application();
			virtual ~Application();

			void Run();

			void OnEvent(Event& e);
		private:
			std::unique_ptr<Window> m_Window;
			bool m_Running = true;
	};
}
