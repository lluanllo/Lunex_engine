#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "Sandbox2D.h"

class Sandbox : public Lunex::Application {
public:
	Sandbox(Lunex::ApplicationCommandLineArgs args)
		: Lunex::Application("Sandbox", args) {
		PushLayer(new Sandbox2D());
	}
	
	~Sandbox() {
	}
};

Lunex::Ref<Lunex::Application> Lunex::CreateApplication(Lunex::ApplicationCommandLineArgs args) {
	return Lunex::CreateRef<Sandbox>(args);
}