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
		
		// ===== ESTILO PROFESIONAL ULTRA-OSCURO (BLENDER/UNREAL INSPIRED) =====
		
		// Espaciado profesional y compacto
		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(6, 4); 
		style.CellPadding = ImVec2(6, 3);
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(4, 4);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 21.0f;
		style.ScrollbarSize = 14.0f;
		style.GrabMinSize = 10.0f;
		
		// Bordes definidos para separación clara
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		
		// Sin redondeado (estilo Blender)
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		
		// Alineación profesional
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
		
		// ===== PALETA DE COLORES PROFESIONAL (BLENDER AZUL) =====
		
		// Fondos ultra-oscuros con jerarquía clara
		const ImVec4 bgVeryDark    = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);  // Fondo más oscuro
		const ImVec4 bgDark        = ImVec4(0.13f, 0.13f, 0.14f, 1.0f);  // Fondo principal
		const ImVec4 bgMedium      = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);  // Áreas elevadas
		const ImVec4 bgLight       = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);  // Controles
		const ImVec4 bgHeader      = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);  // Headers
		
		// Acento AZUL profesional (estilo Blender)
		const ImVec4 accent        = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);  // Azul principal #4296F5
		const ImVec4 accentHover   = ImVec4(0.36f, 0.68f, 1.0f, 1.0f);   // Hover más brillante
		const ImVec4 accentActive  = ImVec4(0.20f, 0.50f, 0.87f, 1.0f);  // Active más oscuro
		const ImVec4 accentDim     = ImVec4(0.26f, 0.59f, 0.98f, 0.50f); // Dim transparente
		const ImVec4 accentSubtle  = ImVec4(0.26f, 0.59f, 0.98f, 0.20f); // Muy sutil
		
		// Texto con excelente contraste
		const ImVec4 text          = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);  // Texto principal
		const ImVec4 textBright    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);     // Texto destacado
		const ImVec4 textDisabled  = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);  // Deshabilitado
		const ImVec4 textDim       = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);  // Secundario
		
		// Bordes sutiles pero visibles
		const ImVec4 border        = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
		const ImVec4 borderLight   = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
		const ImVec4 separator     = ImVec4(0.28f, 0.28f, 0.30f, 0.50f);
		
		// ===== APLICACIÓN DE COLORES =====
		
		// Backgrounds
		colors[ImGuiCol_WindowBg]               = bgDark;
		colors[ImGuiCol_ChildBg]                = bgVeryDark;
		colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.12f, 0.98f);
		colors[ImGuiCol_MenuBarBg]              = bgHeader;
		
		// Borders
		colors[ImGuiCol_Border]                 = border;
		colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		
		// Text
		colors[ImGuiCol_Text]                   = text;
		colors[ImGuiCol_TextDisabled]           = textDisabled;
		colors[ImGuiCol_TextSelectedBg]         = ImVec4(accent.x, accent.y, accent.z, 0.35f);
		
		// Headers
		colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
		colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		colors[ImGuiCol_HeaderActive]           = ImVec4(accent.x, accent.y, accent.z, 0.30f);
		
		// Buttons (estilo flat profesional)
		colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
		colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		colors[ImGuiCol_ButtonActive]           = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
		
		// Frame backgrounds
		colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
		colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
		colors[ImGuiCol_FrameBgActive]          = ImVec4(0.24f, 0.24f, 0.26f, 1.0f);
		
		// Tabs (estilo Blender)
		colors[ImGuiCol_Tab]                    = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
		colors[ImGuiCol_TabHovered]             = ImVec4(0.24f, 0.24f, 0.26f, 1.0f);
		colors[ImGuiCol_TabActive]              = bgDark;
		colors[ImGuiCol_TabUnfocused]           = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
		colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
		
		// Title bar
		colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		colors[ImGuiCol_TitleBgActive]          = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		
		// Scrollbar
		colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.43f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
		
		// Sliders y checks (acento azul)
		colors[ImGuiCol_CheckMark]              = accent;
		colors[ImGuiCol_SliderGrab]             = accent;
		colors[ImGuiCol_SliderGrabActive]       = accentActive;
		
		// Resize grip
		colors[ImGuiCol_ResizeGrip]             = ImVec4(accent.x, accent.y, accent.z, 0.20f);
		colors[ImGuiCol_ResizeGripHovered]      = ImVec4(accent.x, accent.y, accent.z, 0.40f);
		colors[ImGuiCol_ResizeGripActive]       = ImVec4(accent.x, accent.y, accent.z, 0.60f);
		
		// Separator
		colors[ImGuiCol_Separator]              = separator;
		colors[ImGuiCol_SeparatorHovered]       = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		colors[ImGuiCol_SeparatorActive]        = accent;
		
		// Docking
		colors[ImGuiCol_DockingPreview]         = ImVec4(accent.x, accent.y, accent.z, 0.40f);
		colors[ImGuiCol_DockingEmptyBg]         = bgVeryDark;
		
		// Tables
		colors[ImGuiCol_TableHeaderBg]          = bgHeader;
		colors[ImGuiCol_TableBorderStrong]      = border;
		colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.0f);
		colors[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
		
		// Plots
		colors[ImGuiCol_PlotLines]              = accent;
		colors[ImGuiCol_PlotLinesHovered]       = accentHover;
		colors[ImGuiCol_PlotHistogram]          = ImVec4(0.40f, 0.40f, 0.43f, 1.0f);
		colors[ImGuiCol_PlotHistogramHovered]   = accent;
		
		// Drag and drop
		colors[ImGuiCol_DragDropTarget]         = ImVec4(accent.x, accent.y, accent.z, 0.90f);
		
		// Navigation
		colors[ImGuiCol_NavHighlight]           = accent;
		colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
		
		// Modal
		colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
	}
}