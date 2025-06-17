#include <Stellara.h>

class ExampleLayer : public Stellara::Layer{
	public:
		ExampleLayer()
			: Layer("Example") {
		}

		void OnUpdate() override {
			STLR_LOG_INFO("ExampleLayer::Update");
		}

		void OnEvent(Stellara::Event& event) override {
			STLR_LOG_TRACE("{0}", event);
		}

};

class Sandbox : public Stellara::Application{
	public:
		Sandbox() {
			PushLayer(new ExampleLayer());
		}

		~Sandbox() {
		}

};

Stellara::Application* Stellara::CreateApplication() {
	return new Sandbox();
}
