#pragma once

#include "Events/Event.h"

namespace Stellara {

	class Application {
	public:
		Application();
		virtual ~Application();

		void Run();
	};
}
