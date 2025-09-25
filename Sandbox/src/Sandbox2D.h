#pragma once

#include <Lunex.h>

class Sandbox2D : public Lunex::Layer {

	public:
		Sandbox2D();
		virtual ~Sandbox2D() = default;
		
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		
		void OnUpdate(Lunex::Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Lunex::Event& e) override;
		
	private:
		Lunex::OrthographicCameraController m_CameraController;
		
		// Temp
		Lunex::Ref<Lunex::VertexArray> m_SquareVA;
		Lunex::Ref<Lunex::Shader> m_FlatColorShader;
		
		glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
};