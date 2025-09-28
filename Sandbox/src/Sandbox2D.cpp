#include "Sandbox2D.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
}

void Sandbox2D::OnAttach() {
	LNX_PROFILE_FUNCTION();
	m_CheckerboardTexture = Lunex::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach() {
	LNX_PROFILE_FUNCTION();
}

void Sandbox2D::OnUpdate(Lunex::Timestep ts) {
	// Update
	LNX_PROFILE_FUNCTION();
	
	m_CameraController.OnUpdate(ts);
	
	
	{
		LNX_PROFILE_SCOPE("Renderer Prep");
		Lunex::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Lunex::RenderCommand::Clear();
	}
	
	{
		LNX_PROFILE_SCOPE("Renderer Draw");
		Lunex::Renderer2D::BeginScene(m_CameraController.GetCamera());
		Lunex::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
		Lunex::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
		Lunex::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerboardTexture);
		Lunex::Renderer2D::EndScene();
	}
}

void Sandbox2D::OnImGuiRender() {
	LNX_PROFILE_FUNCTION();
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}

void Sandbox2D::OnEvent(Lunex::Event& e) {
	m_CameraController.OnEvent(e);
}