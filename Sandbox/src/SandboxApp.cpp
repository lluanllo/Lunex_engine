#include <Stellara.h>

#include "imgui.h"

class ExampleLayer : public Stellara::Layer{
	public:
		ExampleLayer()
			: Layer("Example") {
		}

		void OnUpdate() override {
			
		}

		
		virtual void OnImGuiRender() override {
			
		}
		

		void OnEvent(Stellara::Event& event) override {
			
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
