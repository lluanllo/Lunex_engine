#include <Stellara.h>

class ExampleLayer : public Stellara::Layer{
	public:
		ExampleLayer()
			: Layer("Example") {
		}

		void OnUpdate() override {
			
			if (Stellara::Input::IsKeyPressed(ST_KEY_TAB)) {
				STLR_LOG_INFO("Tab key is pressed!");
			}
		}

		void OnEvent(Stellara::Event& event) override {
			if (event.GetEventType() == Stellara::EventType::KeyPressed) {
				Stellara::KeyPressedEvent& e = (Stellara::KeyPressedEvent&)event;
				STLR_LOG_TRACE("Key pressed: {0}", (char)e.GetKeyCode());
			}
		}

};

class Sandbox : public Stellara::Application{
	public:
		Sandbox() {
			PushLayer(new ExampleLayer());
			PushOverlay(new Stellara::ImGuiLayer());
		}

		~Sandbox() {
		}

};

Stellara::Application* Stellara::CreateApplication() {
	return new Sandbox();
}
