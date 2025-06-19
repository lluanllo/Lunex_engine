#include "stpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "Platform/OpenGL/imgui_impl_opengl3.h"

#include "Application.h"

// Temporary
#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Stellara {

	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
		
	}

	ImGuiLayer::~ImGuiLayer() {
		
	}

	void ImGuiLayer::OnAttach(){
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		
		ImGuiIO& io = ImGui::GetIO();
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		
		ImGui_ImplOpenGL3_Init("#version 410");
	}

	void ImGuiLayer::OnDetach(){
         
	}

	void ImGuiLayer::OnUpdate() {
		
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());
		
		float time = (float)glfwGetTime();
		io.DeltaTime = m_Time > 0.0f ? (time - m_Time) : (1.0f / 60.0f);
		m_Time = time;
		
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();
		
		static bool show = true;
		ImGui::ShowDemoWindow(&show);
		
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	}

	void ImGuiLayer::OnEvent(Event& event) {
		
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<MouseButtonPressedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonPressedEvent));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonReleasedEvent));
		dispatcher.Dispatch<MouseMovedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnMouseMovedEvent));
		dispatcher.Dispatch<MouseScrolledEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnMouseScrolledEvent));
		dispatcher.Dispatch<KeyPressedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnKeyPressedEvent));
		dispatcher.Dispatch<KeyTypedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnKeyTypedEvent));
		dispatcher.Dispatch<KeyReleasedEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnKeyReleasedEvent));
		dispatcher.Dispatch<WindowResizeEvent>(STLR_BIND_EVENT_FN(ImGuiLayer::OnWindowResizeEvent));
	}

	bool ImGuiLayer::OnMouseButtonPressedEvent(MouseButtonPressedEvent& e) {
		
		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseButtonEvent(e.GetMouseButton(), true);
		return false;
	}

	bool ImGuiLayer::OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e) {
		
		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseButtonEvent(e.GetMouseButton(), false);
		return false;
	}

	bool ImGuiLayer::OnMouseMovedEvent(MouseMovedEvent& e) {
		
		ImGuiIO& io = ImGui::GetIO();		
		io.AddMousePosEvent(e.GetX(), e.GetY());
		return false;
	}

	bool ImGuiLayer::OnMouseScrolledEvent(MouseScrolledEvent& e){
		
		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseWheelEvent(e.GetXOffset(), e.GetYOffset());
		return false;
	}

	bool ImGuiLayer::OnKeyPressedEvent(KeyPressedEvent& e) {
		ImGuiIO& io = ImGui::GetIO();
		int keycode = e.GetKeyCode();
		io.AddKeyEvent(ImGuiKey(keycode), true);
		
		io.AddKeyEvent(ImGuiMod_Ctrl, e.GetKeyCode() == GLFW_KEY_LEFT_CONTROL || e.GetKeyCode() == GLFW_KEY_RIGHT_CONTROL);
		io.AddKeyEvent(ImGuiMod_Shift, e.GetKeyCode() == GLFW_KEY_LEFT_SHIFT || e.GetKeyCode() == GLFW_KEY_RIGHT_SHIFT);
		io.AddKeyEvent(ImGuiMod_Alt, e.GetKeyCode() == GLFW_KEY_LEFT_ALT || e.GetKeyCode() == GLFW_KEY_RIGHT_ALT);
		io.AddKeyEvent(ImGuiMod_Super, e.GetKeyCode() == GLFW_KEY_LEFT_SUPER || e.GetKeyCode() == GLFW_KEY_RIGHT_SUPER);

		return false;
	}

	bool ImGuiLayer::OnKeyReleasedEvent(KeyReleasedEvent& e) {
		ImGuiIO& io = ImGui::GetIO();
		int keycode = e.GetKeyCode();
		io.AddKeyEvent(ImGuiKey(keycode), false);
		
		return false;
	}
	bool ImGuiLayer::OnKeyTypedEvent(KeyTypedEvent& e) {
		
		ImGuiIO& io = ImGui::GetIO();
		int keycode = e.GetKeyCode();
		
		if (keycode > 0 && keycode < 0x10000) {
			io.AddInputCharacter((unsigned short)keycode);
		}
		
		return false;
	}

	bool ImGuiLayer::OnWindowResizeEvent(WindowResizeEvent& e){
		
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(e.GetWidth(), e.GetHeight());
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
		glViewport(0, 0, e.GetWidth(), e.GetHeight());
		
		return false;
	}

}