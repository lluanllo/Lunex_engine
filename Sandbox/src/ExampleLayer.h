#pragma once

#include <Lunex.h>

class ExampleLayer : public Lunex::Layer {
	public:
		ExampleLayer();
		virtual ~ExampleLayer() = default;
		
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		
		void OnUpdate(Lunex::Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Lunex::Event& e) override;
	private:
		Lunex::ShaderLibrary m_ShaderLibrary;
		Lunex::Ref<Lunex::Shader> m_Shader;
		Lunex::Ref<Lunex::VertexArray> m_VertexArray;
		
		Lunex::Ref<Lunex::Shader> m_FlatColorShader;
		Lunex::Ref<Lunex::VertexArray> m_SquareVA;
		
		Lunex::Ref<Lunex::Texture2D> m_Texture, m_ChernoLogoTexture;
		
		Lunex::OrthographicCameraController m_CameraController;
		glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};