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
	
	void ImGuiLayer::SetDarkThemeColor() {
		auto& colors = ImGui::GetStyle().Colors;
		auto& style = ImGui::GetStyle();
		
		// ===== ESTILO PROFESIONAL INSPIRADO EN UNREAL ENGINE =====
		
		// Espaciado y padding más refinado
		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(5, 3);
		style.CellPadding = ImVec2(6, 3);
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(4, 4);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 20.0f;
		style.ScrollbarSize = 14.0f;
		style.GrabMinSize = 10.0f;
		
		// Bordes sutiles pero presentes
		style.WindowBorderSize = 0.0f;
		style.ChildBorderSize = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		
		// Redondeado sutil y profesional (menos que antes)
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 3.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 2.0f;
		
		// Alineación profesional
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
		
		// Otros ajustes
		style.DisplaySafeAreaPadding = ImVec2(3, 3);
		
		// ===== PALETA DE COLORES PROFESIONAL (UNREAL-INSPIRED) =====
		
		// Fondos con gradientes sutiles y profundidad
		const ImVec4 bgVeryDark = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);      // Fondo más oscuro
		const ImVec4 bgDark = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);          // Fondo principal
		const ImVec4 bgMedium = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);        // Áreas elevadas
		const ImVec4 bgLight = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);         // Controles
		const ImVec4 bgLighter = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);       // Hover sutil
		
		// Acento naranja refinado (menos saturado, más profesional)
		const ImVec4 accent = ImVec4(0.90f, 0.50f, 0.10f, 1.0f);          // Naranja principal
		const ImVec4 accentHover = ImVec4(1.0f, 0.60f, 0.20f, 1.0f);      // Hover más brillante
		const ImVec4 accentActive = ImVec4(0.80f, 0.45f, 0.08f, 1.0f);    // Active más oscuro
		const ImVec4 accentDim = ImVec4(0.60f, 0.35f, 0.08f, 1.0f);       // Dim para secundarios
		const ImVec4 accentSubtle = ImVec4(0.90f, 0.50f, 0.10f, 0.30f);   // Transparente
		
		// Texto con mejor jerarquía
		const ImVec4 text = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);            // Texto principal
		const ImVec4 textBright = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);         // Texto destacado
		const ImVec4 textDisabled = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);    // Texto deshabilitado
		const ImVec4 textDim = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);         // Texto secundario
		
		// Bordes y separadores sutiles
		const ImVec4 border = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
		const ImVec4 borderLight = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
		const ImVec4 separator = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
		
		// ===== APLICACIÓN DE COLORES =====
		
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
		
		// Headers (más sutiles, menos saturados)
		colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
		colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
		
		// Buttons (más planos y profesionales)
		colors[ImGuiCol_Button] = bgLight;
		colors[ImGuiCol_ButtonHovered] = bgLighter;
		colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		
		// Frame backgrounds (inputs, etc.)
		colors[ImGuiCol_FrameBg] = bgVeryDark;
		colors[ImGuiCol_FrameBgHovered] = bgMedium;
		colors[ImGuiCol_FrameBgActive] = bgLight;
		
		// Tabs (más refinados)
		colors[ImGuiCol_Tab] = bgMedium;
		colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
		colors[ImGuiCol_TabActive] = bgLight;
		colors[ImGuiCol_TabUnfocused] = bgDark;
		colors[ImGuiCol_TabUnfocusedActive] = bgMedium;
		
		// Title bar (más sutil)
		colors[ImGuiCol_TitleBg] = bgVeryDark;
		colors[ImGuiCol_TitleBgActive] = bgDark;
		colors[ImGuiCol_TitleBgCollapsed] = bgVeryDark;
		
		// Scrollbar (más elegante)
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_ScrollbarGrab] = bgLight;
		colors[ImGuiCol_ScrollbarGrabHovered] = bgLighter;
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
		
		// Sliders y checks (acento naranja refinado)
		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentHover;
		
		// Resize grip
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		
		// Separator
		colors[ImGuiCol_Separator] = separator;
		colors[ImGuiCol_SeparatorHovered] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		colors[ImGuiCol_SeparatorActive] = accent;
		
		// Docking
		colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
		colors[ImGuiCol_DockingEmptyBg] = bgVeryDark;
		
		// Tables
		colors[ImGuiCol_TableHeaderBg] = bgMedium;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
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
}