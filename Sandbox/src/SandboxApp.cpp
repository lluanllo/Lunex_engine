#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "Sandbox2D.h"

class Sandbox : public Lunex::Application{
	public:
		Sandbox() {
			PushLayer(new Sandbox2D());
		}
		
		~Sandbox() {
		}
};

Lunex::Ref <Lunex::Application> Lunex::CreateApplication() {
	return Lunex::CreateScope<Sandbox>();
}