#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "Sandbox2D.h"

class Sandbox : public Lunex::Application {
public:
	Sandbox() {
		PushLayer(new Sandbox2D());
	}
	
	~Sandbox() {
	}
};

// Match signature expected by EntryPoint (accept ApplicationCommandLineArgs)
Lunex::Ref<Lunex::Application> Lunex::CreateApplication(Lunex::ApplicationCommandLineArgs args) {
	// args can be used to configure the app if needed
	return Lunex::CreateScope<Sandbox>();
}