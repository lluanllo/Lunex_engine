#include "stpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h"

#include "Core/Application.h"

// TEMPORARY
#include <GLFW/glfw3.h>

#include "ImGuizmo.h"

namespace Lunex {	
	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {
		
	}
	
	ImGuiLayer::~ImGuiLayer() {
		
	}
	
	void ImGuiLayer::OnAttach() {
		LNX_PROFILE_FUNCTION();
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		
		float m_FontSize = 15.0f;  // ✅ Ligeramente más pequeño para look profesional
		
		io.Fonts->AddFontFromFileTTF("assets/Fonts/JetBrainsMono/JetBrainsMono-Bold.ttf", m_FontSize);
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/Fonts/JetBrainsMono/JetBrainsMono-Regular.ttf", m_FontSize);
		
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		
		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
		
		SetDarkThemeColor();
		
		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
		
		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410");
	}
	
	void ImGuiLayer::OnDetach() {
		LNX_PROFILE_FUNCTION();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	
	void ImGuiLayer::OnEvent(Event& e) {
		if (m_BlockEvents) {
			ImGuiIO& io = ImGui::GetIO();
			e.m_Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.m_Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}
	
	void ImGuiLayer::Begin() {
		LNX_PROFILE_FUNCTION();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}
	
	void ImGuiLayer::End() {
		LNX_PROFILE_FUNCTION();
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());
		
		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}
	
	// Add this to your ImGuiLayer.cpp SetDarkThemeColor() method
	// These are ADDITIONAL refinements to make it even more UE5-like
	
	void ImGuiLayer::SetDarkThemeColor() {
		auto& colors = ImGui::GetStyle().Colors;
		auto& style = ImGui::GetStyle();
		
		// ===== SPACING REFINEMENTS FOR UE5 LOOK =====
		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(6, 4);      // Slightly more padding for buttons
		style.CellPadding = ImVec2(8, 4);       // Better table spacing
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 20.0f;
		style.ScrollbarSize = 14.0f;
		style.GrabMinSize = 10.0f;
		
		// Borders
		style.WindowBorderSize = 1.0f;          // Thin border for windows
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		
		// Rounding (very subtle, UE5 style)
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 3.0f;             // Slightly more rounded
		style.PopupRounding = 3.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		
		// Alignment
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
		
		// ===== UE5-INSPIRED COLOR PALETTE =====
		
		// Ultra-dark backgrounds (like UE5)
		const ImVec4 bgVeryDark = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
		const ImVec4 bgDark = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
		const ImVec4 bgMedium = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
		const ImVec4 bgLight = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
		const ImVec4 bgLighter = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
		
		// Orange accent (UE5 style - less saturated)
		const ImVec4 accent = ImVec4(0.90f, 0.50f, 0.10f, 1.0f);
		const ImVec4 accentHover = ImVec4(1.0f, 0.60f, 0.20f, 1.0f);
		const ImVec4 accentActive = ImVec4(0.80f, 0.45f, 0.08f, 1.0f);
		const ImVec4 accentDim = ImVec4(0.60f, 0.35f, 0.08f, 1.0f);
		
		// Text hierarchy
		const ImVec4 text = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
		const ImVec4 textBright = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		const ImVec4 textDisabled = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
		const ImVec4 textDim = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
		
		// Borders (very subtle)
		const ImVec4 border = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
		const ImVec4 borderLight = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
		
		// ===== APPLY COLORS =====
		
		// Backgrounds
		colors[ImGuiCol_WindowBg] = bgDark;
		colors[ImGuiCol_ChildBg] = bgVeryDark;
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);
		colors[ImGuiCol_MenuBarBg] = bgMedium;
		
		// Borders
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		
		// Text
		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textDisabled;
		colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
		
		// Headers (flat style like UE5)
		colors[ImGuiCol_Header] = bgMedium;
		colors[ImGuiCol_HeaderHovered] = bgLight;
		colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
		
		// Buttons (flat, like UE5)
		colors[ImGuiCol_Button] = bgMedium;
		colors[ImGuiCol_ButtonHovered] = bgLight;
		colors[ImGuiCol_ButtonActive] = bgLighter;
		
		// Frame backgrounds
		colors[ImGuiCol_FrameBg] = bgVeryDark;
		colors[ImGuiCol_FrameBgHovered] = bgMedium;
		colors[ImGuiCol_FrameBgActive] = bgLight;
		
		// Tabs (cleaner look)
		colors[ImGuiCol_Tab] = bgMedium;
		colors[ImGuiCol_TabHovered] = bgLight;
		colors[ImGuiCol_TabActive] = bgLighter;
		colors[ImGuiCol_TabUnfocused] = bgDark;
		colors[ImGuiCol_TabUnfocusedActive] = bgMedium;
		
		// Title bar
		colors[ImGuiCol_TitleBg] = bgVeryDark;
		colors[ImGuiCol_TitleBgActive] = bgDark;
		colors[ImGuiCol_TitleBgCollapsed] = bgVeryDark;
		
		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_ScrollbarGrab] = bgLight;
		colors[ImGuiCol_ScrollbarGrabHovered] = bgLighter;
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
		
		// Sliders and checkmarks (orange accent)
		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentHover;
		
		// Resize grip
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		
		// Separator
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		colors[ImGuiCol_SeparatorActive] = accent;
		
		// Docking
		colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
		colors[ImGuiCol_DockingEmptyBg] = bgVeryDark;
		
		// Tables (cleaner borders)
		colors[ImGuiCol_TableHeaderBg] = bgMedium;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
		
		// Plots
		colors[ImGuiCol_PlotLines] = accent;
		colors[ImGuiCol_PlotLinesHovered] = accentHover;
		colors[ImGuiCol_PlotHistogram] = accentDim;
		colors[ImGuiCol_PlotHistogramHovered] = accent;
		
		// Drag and drop
		colors[ImGuiCol_DragDropTarget] = ImVec4(accent.x, accent.y, accent.z, 0.80f);
		
		// Navigation
		colors[ImGuiCol_NavHighlight] = accent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
		
		// Modal
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);
	}
	
	// OPTIONAL: Additional UE5-like improvements for the entire editor
	
	// Add this to make separators more visible (call after SetDarkThemeColor)
	void ImGuiLayer::ApplyUE5Refinements() {
		auto& style = ImGui::GetStyle();
		
		// Make separators slightly thicker and more visible
		style.SeparatorTextBorderSize = 1.0f;
		
		// Adjust alpha for better visibility
		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.5f;
		
		// Window settings
		style.WindowMinSize = ImVec2(100, 100);
	}
}