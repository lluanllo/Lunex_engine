#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "Sandbox2D.h"

namespace Lunex {
	class Sandbox : public Application {
	public:
		Sandbox(ApplicationCommandLineArgs args)
			: Application("Sandbox", args) {
			PushLayer(new Sandbox2D());
		}
		
		~Sandbox() {
		}
	};

	Ref<Application> CreateApplication(ApplicationCommandLineArgs args) {
		return CreateRef<Sandbox>(args);
	}
}