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
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;
		
		float m_FontSize = 16.0f;
		
		io.Fonts->AddFontFromFileTTF("assets/Fonts/JetBrainsMono/JetBrainsMono-Bold.ttf", m_FontSize);
		
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/Fonts/JetBrainsMono/JetBrainsMono-Regular.ttf", m_FontSize);
		
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();
		
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
	
	void ImGuiLayer::SetDarkThemeColor() {
		auto& colors = ImGui::GetStyle().Colors;
		auto& style = ImGui::GetStyle();
		
		// ===== CONFIGURACIÓN DE ESTILO MEJORADO =====
		style.WindowPadding = ImVec2(10, 10);
		style.WindowRounding = 4.0f;
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);  // Título centrado
		style.FramePadding = ImVec2(6, 4);
		style.FrameRounding = 3.0f;
		style.ItemSpacing = ImVec2(10, 6);
		style.ItemInnerSpacing = ImVec2(6, 5);
		style.IndentSpacing = 22.0f;
		style.ScrollbarSize = 16.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabMinSize = 12.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.PopupRounding = 3.0f;
		
		// Bordes más definidos
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		
		// ===== PALETA DE COLORES MEJORADA =====
		// Fondo principal más oscuro y limpio
		const ImVec4 bgDarkest = ImVec4{ 0.08f, 0.08f, 0.09f, 1.0f };
		const ImVec4 bgDark = ImVec4{ 0.11f, 0.11f, 0.12f, 1.0f };
		const ImVec4 bgMedium = ImVec4{ 0.14f, 0.14f, 0.15f, 1.0f };
		const ImVec4 bgLight = ImVec4{ 0.18f, 0.18f, 0.19f, 1.0f };
		const ImVec4 bgLighter = ImVec4{ 0.22f, 0.22f, 0.23f, 1.0f };
		
		// Acento naranja más vibrante
		const ImVec4 accent = ImVec4{ 1.0f, 0.60f, 0.0f, 1.0f };
		const ImVec4 accentHover = ImVec4{ 1.0f, 0.70f, 0.20f, 1.0f };
		const ImVec4 accentActive = ImVec4{ 1.0f, 0.55f, 0.0f, 1.0f };
		const ImVec4 accentDim = ImVec4{ 0.7f, 0.42f, 0.0f, 1.0f };
		
		// Selección y hover mejorados
		const ImVec4 selection = ImVec4{ 0.26f, 0.26f, 0.27f, 1.0f };
		const ImVec4 selectionHover = ImVec4{ 0.32f, 0.32f, 0.33f, 1.0f };
		const ImVec4 selectionActive = ImVec4{ 0.38f, 0.38f, 0.39f, 1.0f };
		
		// Texto con mejor contraste
		const ImVec4 text = ImVec4{ 0.95f, 0.95f, 0.95f, 1.0f };
		const ImVec4 textDisabled = ImVec4{ 0.50f, 0.50f, 0.50f, 1.0f };
		const ImVec4 textBright = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
		
		// Bordes con contraste sutil
		const ImVec4 border = ImVec4{ 0.20f, 0.20f, 0.21f, 1.0f };
		const ImVec4 borderLight = ImVec4{ 0.28f, 0.28f, 0.29f, 1.0f };
		const ImVec4 borderAccent = ImVec4{ 0.40f, 0.28f, 0.10f, 1.0f };
		
		// ===== APLICACIÓN DE COLORES =====
		
		// Windows con gradiente sutil
		colors[ImGuiCol_WindowBg] = bgDark;
		colors[ImGuiCol_ChildBg] = bgMedium;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.10f, 0.10f, 0.11f, 0.98f };
		
		// Headers mejorados con acento naranja
		colors[ImGuiCol_Header] = ImVec4{ accent.x, accent.y, accent.z, 0.35f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ accent.x, accent.y, accent.z, 0.50f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ accent.x, accent.y, accent.z, 0.65f };
		
		// Buttons con mejor feedback visual
		colors[ImGuiCol_Button] = bgLight;
		colors[ImGuiCol_ButtonHovered] = bgLighter;
		colors[ImGuiCol_ButtonActive] = selection;
		
		// Frame BG con más profundidad
		colors[ImGuiCol_FrameBg] = bgDarkest;
		colors[ImGuiCol_FrameBgHovered] = bgMedium;
		colors[ImGuiCol_FrameBgActive] = bgLight;
		
		// Tabs con mejor separación visual
		colors[ImGuiCol_Tab] = bgDark;
		colors[ImGuiCol_TabHovered] = bgLighter;
		colors[ImGuiCol_TabActive] = bgMedium;
		colors[ImGuiCol_TabUnfocused] = bgDarkest;
		colors[ImGuiCol_TabUnfocusedActive] = bgDark;
		
		// Title Bar con acento
		colors[ImGuiCol_TitleBg] = bgDarkest;
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.12f, 0.12f, 0.13f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = bgDarkest;
		
		// Scrollbar elegante
		colors[ImGuiCol_ScrollbarBg] = bgDark;
		colors[ImGuiCol_ScrollbarGrab] = bgLight;
		colors[ImGuiCol_ScrollbarGrabHovered] = bgLighter;
		colors[ImGuiCol_ScrollbarGrabActive] = selection;
		
		// Check Mark y Slider con acento naranja brillante
		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentHover;
		
		// Resize Grip más visible
		colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.26f, 0.26f, 0.27f, 0.60f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.32f, 0.32f, 0.33f, 0.80f };
		colors[ImGuiCol_ResizeGripActive] = ImVec4{ accent.x, accent.y, accent.z, 0.95f };
		
		// Plot con colores vibrantes
		colors[ImGuiCol_PlotLines] = accent;
		colors[ImGuiCol_PlotLinesHovered] = accentHover;
		colors[ImGuiCol_PlotHistogram] = accentDim;
		colors[ImGuiCol_PlotHistogramHovered] = accent;
		
		// Text
		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textDisabled;
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ accent.x, accent.y, accent.z, 0.40f };
		
		// Border con mejor contraste
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.3f };
		
		// Separator más visible
		colors[ImGuiCol_Separator] = borderLight;
		colors[ImGuiCol_SeparatorHovered] = ImVec4{ accent.x, accent.y, accent.z, 0.60f };
		colors[ImGuiCol_SeparatorActive] = accent;
		
		// Docking con acento
		colors[ImGuiCol_DockingPreview] = ImVec4{ accent.x, accent.y, accent.z, 0.40f };
		colors[ImGuiCol_DockingEmptyBg] = bgDark;
		
		// Table mejorada
		colors[ImGuiCol_TableHeaderBg] = bgMedium;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.16f, 0.16f, 0.17f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.03f };
		
		// Drag and Drop más evidente
		colors[ImGuiCol_DragDropTarget] = ImVec4{ accent.x, accent.y, accent.z, 0.90f };
		
		// Nav con acento
		colors[ImGuiCol_NavHighlight] = accent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.70f };
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.65f };
		
		// Modal window dimming
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.75f };
	}
}