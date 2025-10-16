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
		
		float m_FontSize = 15.0f;
		
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
		
		// ===== CONFIGURACIÓN DE BORDES Y ESTILO TIPO UNREAL =====
		style.WindowPadding = ImVec2(8, 8);
		style.WindowRounding = 2.0f;  // Unreal usa esquinas rectas
		style.FramePadding = ImVec2(5, 3);
		style.FrameRounding = 2.0f;  // Bordes muy sutiles
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.IndentSpacing = 21.0f;
		style.ScrollbarSize = 14.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabMinSize = 10.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.PopupRounding = 0.0f;
		
		// Bordes
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		
		// ===== PALETA DE COLORES ESTILO UNREAL ENGINE =====
		// Grises oscuros con acentos naranjas/amarillos
		
		// Colores base - Grises muy oscuros
		const ImVec4 unrealDarkest = ImVec4{ 0.04f, 0.04f, 0.04f, 1.0f };        // Negro casi puro
		const ImVec4 unrealDarkBg = ImVec4{ 0.06f, 0.06f, 0.06f, 1.0f };         // Fondo principal
		const ImVec4 unrealMediumBg = ImVec4{ 0.09f, 0.09f, 0.09f, 1.0f };       // Fondo medio
		const ImVec4 unrealLightBg = ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f };        // Fondo claro
		const ImVec4 unrealLightest = ImVec4{ 0.18f, 0.18f, 0.18f, 1.0f };       // Gris más claro
		
		// Acentos - Naranja/Amarillo característico de Unreal
		const ImVec4 unrealAccent = ImVec4{ 1.0f, 0.55f, 0.0f, 1.0f };           // Naranja brillante
		const ImVec4 unrealAccentHover = ImVec4{ 1.0f, 0.65f, 0.2f, 1.0f };      // Naranja hover
		const ImVec4 unrealAccentActive = ImVec4{ 0.9f, 0.45f, 0.0f, 1.0f };     // Naranja activo
		const ImVec4 unrealAccentDim = ImVec4{ 0.6f, 0.35f, 0.0f, 1.0f };        // Naranja oscuro
		
		// Selección - Gris claro brillante para selecciones
		const ImVec4 unrealSelection = ImVec4{ 0.28f, 0.28f, 0.28f, 1.0f };      // Gris selección
		const ImVec4 unrealSelectionHover = ImVec4{ 0.35f, 0.35f, 0.35f, 1.0f }; // Gris hover
		const ImVec4 unrealSelectionActive = ImVec4{ 0.42f, 0.42f, 0.42f, 1.0f };// Gris activo
		
		// Texto
		const ImVec4 unrealText = ImVec4{ 0.90f, 0.90f, 0.90f, 1.0f };           // Texto principal
		const ImVec4 unrealTextDisabled = ImVec4{ 0.45f, 0.45f, 0.45f, 1.0f };   // Texto deshabilitado
		const ImVec4 unrealTextBright = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };        // Texto brillante
		
		// Bordes
		const ImVec4 unrealBorder = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };         // Borde sutil
		const ImVec4 unrealBorderLight = ImVec4{ 0.20f, 0.20f, 0.20f, 1.0f };    // Borde claro
		
		// ===== APLICACIÓN DE COLORES =====
		
		// Windows
		colors[ImGuiCol_WindowBg] = unrealDarkBg;
		colors[ImGuiCol_ChildBg] = unrealMediumBg;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.08f, 0.08f, 0.08f, 0.98f };
		
		// Headers (TreeNodes, Collapsibles) - HOVER BRILLANTE PARA HIERARCHY
		colors[ImGuiCol_Header] = unrealSelection;                               // Seleccionado
		colors[ImGuiCol_HeaderHovered] = unrealSelectionHover;                   // Hover MÁS BRILLANTE
		colors[ImGuiCol_HeaderActive] = unrealSelectionActive;                   // Activo
		
		// Buttons
		colors[ImGuiCol_Button] = unrealLightBg;
		colors[ImGuiCol_ButtonHovered] = unrealLightest;
		colors[ImGuiCol_ButtonActive] = unrealSelection;
		
		// Frame BG (inputs, checkboxes, etc.)
		colors[ImGuiCol_FrameBg] = unrealDarkest;
		colors[ImGuiCol_FrameBgHovered] = unrealMediumBg;
		colors[ImGuiCol_FrameBgActive] = unrealLightBg;
		
		// Tabs
		colors[ImGuiCol_Tab] = unrealDarkBg;
		colors[ImGuiCol_TabHovered] = unrealLightBg;
		colors[ImGuiCol_TabActive] = unrealMediumBg;
		colors[ImGuiCol_TabUnfocused] = unrealDarkBg;
		colors[ImGuiCol_TabUnfocusedActive] = unrealDarkBg;
		
		// Title Bar
		colors[ImGuiCol_TitleBg] = unrealDarkest;
		colors[ImGuiCol_TitleBgActive] = unrealDarkBg;
		colors[ImGuiCol_TitleBgCollapsed] = unrealDarkest;
		
		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = unrealDarkBg;
		colors[ImGuiCol_ScrollbarGrab] = unrealLightBg;
		colors[ImGuiCol_ScrollbarGrabHovered] = unrealLightest;
		colors[ImGuiCol_ScrollbarGrabActive] = unrealSelection;
		
		// Check Mark & Radio Button - Acento naranja
		colors[ImGuiCol_CheckMark] = unrealAccent;
		
		// Slider - Acento naranja
		colors[ImGuiCol_SliderGrab] = unrealAccent;
		colors[ImGuiCol_SliderGrabActive] = unrealAccentHover;
		
		// Resize Grip
		colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.20f, 0.20f, 0.20f, 0.5f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.28f, 0.28f, 0.28f, 0.67f };
		colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.35f, 0.35f, 0.35f, 0.95f };
		
		// Plot (gráficos)
		colors[ImGuiCol_PlotLines] = unrealAccent;
		colors[ImGuiCol_PlotLinesHovered] = unrealAccentHover;
		colors[ImGuiCol_PlotHistogram] = unrealAccentDim;
		colors[ImGuiCol_PlotHistogramHovered] = unrealAccent;
		
		// Text
		colors[ImGuiCol_Text] = unrealText;
		colors[ImGuiCol_TextDisabled] = unrealTextDisabled;
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ 1.0f, 0.55f, 0.0f, 0.35f };    // Selección con acento naranja
		
		// Border
		colors[ImGuiCol_Border] = unrealBorder;
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
		
		// Separator (líneas divisorias)
		colors[ImGuiCol_Separator] = unrealBorder;
		colors[ImGuiCol_SeparatorHovered] = unrealBorderLight;
		colors[ImGuiCol_SeparatorActive] = unrealLightest;
		
		// Docking
		colors[ImGuiCol_DockingPreview] = ImVec4{ 1.0f, 0.55f, 0.0f, 0.3f };
		colors[ImGuiCol_DockingEmptyBg] = unrealDarkBg;
		
		// Table
		colors[ImGuiCol_TableHeaderBg] = unrealMediumBg;
		colors[ImGuiCol_TableBorderStrong] = unrealBorder;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.02f };
		
		// Drag and Drop
		colors[ImGuiCol_DragDropTarget] = unrealAccentHover;
		
		// Nav (navegación con teclado/gamepad)
		colors[ImGuiCol_NavHighlight] = unrealAccent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.70f };
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.60f };
		
		// Modal window dimming
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.70f };
	}
}