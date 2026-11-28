#include "stpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h"

#include "Core/Application.h"
#include "RHI/RHI.h"
#include "RHI/Vulkan/VulkanContext.h"
#include "RHI/Vulkan/VulkanTexture.h"
#include "backends/imgui_impl_vulkan.h"

// TEMPORARY
#include <GLFW/glfw3.h>

#include "ImGuizmo.h"

namespace Lunex {	
	// ---- Vulkan: engine texture IDs -> descriptor sets ----
	static bool IsVulkan() { return RHI::GetCurrentAPI() == RHI::GraphicsAPI::Vulkan; }
	// Leaked on purpose (static destruction runs after the Vulkan context is gone)
	static Ref<RHI::Vk::VulkanTexture2D>& s_FallbackTexture = *new Ref<RHI::Vk::VulkanTexture2D>();
	static uint64_t s_FallbackSet = 0;

	// ImGui::Image() gets engine texture IDs (like GL names). Vulkan needs descriptor sets: translate
	// every user texture of the frame; IDs of systems not ported yet show a neutral texture.
	static void TranslateTextures(ImDrawData* drawData) {
		if (!drawData) return;
		auto& vk = RHI::Vk::VulkanContext::Get();
		for (ImDrawList* list : drawData->CmdLists) {
			for (ImDrawCmd& cmd : list->CmdBuffer) {
				if (cmd.UserCallback || cmd.TexRef._TexData) continue;
				const uint64_t id = cmd.TexRef._TexID;
				uint64_t set = vk.ResolveImGuiTexture(id);
				if (set == id || set == 0) set = s_FallbackSet;
				cmd.TexRef._TexID = set;
			}
		}
	}

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
		if (IsVulkan()) {
			auto& vk = RHI::Vk::VulkanContext::Get();
			ImGui_ImplGlfw_InitForVulkan(window, true);
			static VkFormat s_SwapchainFormat = vk.GetSwapchainFormat();
			ImGui_ImplVulkan_InitInfo info{};
			info.ApiVersion = VK_API_VERSION_1_3;
			info.Instance = vk.GetInstance();
			info.PhysicalDevice = vk.GetPhysicalDevice();
			info.Device = vk.GetDevice();
			info.QueueFamily = vk.GetQueueFamily();
			info.Queue = vk.GetQueue();
			info.DescriptorPoolSize = 4096;   // icons, thumbnails, viewport images
			info.MinImageCount = 2;
			info.ImageCount = std::max(2u, vk.GetSwapchainImageCount());
			info.UseDynamicRendering = true;
			info.PipelineInfoMain.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
			info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &s_SwapchainFormat;
			ImGui_ImplVulkan_Init(&info);
			vk.SetImGuiReady(true);

			RHI::TextureDesc desc(1, 1, RHI::TextureFormat::RGBA8);
			const uint32_t grey = 0xFF1E1E1E;
			s_FallbackTexture = CreateRef<RHI::Vk::VulkanTexture2D>(desc, &grey, sizeof(grey));
			s_FallbackSet = vk.ResolveImGuiTexture(s_FallbackTexture->GetRendererID());
		}
		else {
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 410");
		}
	}
	
	void ImGuiLayer::OnDetach() {
		LNX_PROFILE_FUNCTION();
		if (IsVulkan()) {
			RHI::Vk::VulkanContext::Get().WaitIdle();
			RHI::Vk::VulkanContext::Get().SetImGuiReady(false);
			ImGui_ImplVulkan_Shutdown();
		}
		else {
			ImGui_ImplOpenGL3_Shutdown();
		}
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
		if (IsVulkan()) ImGui_ImplVulkan_NewFrame();
		else ImGui_ImplOpenGL3_NewFrame();
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
		if (IsVulkan()) {
			TranslateTextures(ImGui::GetDrawData());
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				ImGui::UpdatePlatformWindows();
				for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
					if (viewport != ImGui::GetMainViewport()) TranslateTextures(viewport->DrawData);
			}
			RHI::Vk::VulkanContext::Get().EndFrame([](VkCommandBuffer cmd) {
				ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
			});
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ImGui::RenderPlatformWindowsDefault();
			return;
		}
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
		
		// ===== LUNEX DARK THEME - Blue-tinted with Teal/Cyan accent =====
		
		// Compact professional spacing
		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(6, 4); 
		style.CellPadding = ImVec2(6, 3);
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(4, 4);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 21.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 10.0f;
		
		// Clean border definition
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		
		// Slightly rounded (modern feel)
		style.WindowRounding = 0.0f;
		style.ChildRounding = 2.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 9.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 2.0f;
		
		// Professional alignment
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
		
		// ===== COLOR PALETTE (UNREAL-STYLE NEUTRAL GRAYS + WARM ACCENT) =====
		// Change the accent here; every other color is a neutral gray.

		// Neutral dark backgrounds (no hue)
		const ImVec4 bgVeryDark    = ImVec4(0.082f, 0.082f, 0.082f, 1.0f);   // #151515
		const ImVec4 bgDark        = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);   // #1A1A1A
		const ImVec4 bgMedium      = ImVec4(0.141f, 0.141f, 0.141f, 1.0f);   // #242424
		const ImVec4 bgLight       = ImVec4(0.184f, 0.184f, 0.184f, 1.0f);   // #2F2F2F
		const ImVec4 bgHeader      = ImVec4(0.122f, 0.122f, 0.122f, 1.0f);   // #1F1F1F

		// Warm accent (selection / active elements)
		const ImVec4 accent        = ImVec4(0.851f, 0.510f, 0.169f, 1.0f);   // #D9822B
		const ImVec4 accentHover   = ImVec4(0.922f, 0.588f, 0.271f, 1.0f);   // #EB9645
		const ImVec4 accentActive  = ImVec4(0.722f, 0.416f, 0.122f, 1.0f);   // #B86A1F
		const ImVec4 accentDim     = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		const ImVec4 accentSubtle  = ImVec4(accent.x, accent.y, accent.z, 0.12f);

		// Text
		const ImVec4 text          = ImVec4(0.86f, 0.86f, 0.86f, 1.0f);      // Primary text
		const ImVec4 textBright    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);         // Highlighted
		const ImVec4 textDisabled  = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);      // Disabled
		const ImVec4 textDim       = ImVec4(0.56f, 0.56f, 0.56f, 1.0f);      // Secondary

		// Borders
		const ImVec4 border        = ImVec4(0.055f, 0.055f, 0.055f, 1.0f);   // #0E0E0E
		const ImVec4 borderLight   = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);   // #383838
		const ImVec4 separator     = ImVec4(0.220f, 0.220f, 0.220f, 0.60f);

		// ===== APPLY COLORS =====
		
		// Backgrounds
		colors[ImGuiCol_WindowBg]               = bgDark;
		colors[ImGuiCol_ChildBg]                = bgVeryDark;
		colors[ImGuiCol_PopupBg]                = ImVec4(0.107f, 0.107f, 0.107f, 0.98f);
		colors[ImGuiCol_MenuBarBg]              = bgHeader;
		
		// Borders
		colors[ImGuiCol_Border]                 = border;
		colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		
		// Text
		colors[ImGuiCol_Text]                   = text;
		colors[ImGuiCol_TextDisabled]           = textDisabled;
		colors[ImGuiCol_TextSelectedBg]         = ImVec4(accent.x, accent.y, accent.z, 0.30f);
		
		// Headers
		colors[ImGuiCol_Header]                 = ImVec4(0.144f, 0.144f, 0.144f, 1.0f);
		colors[ImGuiCol_HeaderHovered]          = ImVec4(0.194f, 0.194f, 0.194f, 1.0f);
		colors[ImGuiCol_HeaderActive]           = ImVec4(accent.x, accent.y, accent.z, 0.25f);
		
		// Buttons (blue-tinted dark)
		colors[ImGuiCol_Button]                 = ImVec4(0.167f, 0.167f, 0.167f, 1.0f);
		colors[ImGuiCol_ButtonHovered]          = ImVec4(0.227f, 0.227f, 0.227f, 1.0f);
		colors[ImGuiCol_ButtonActive]           = ImVec4(0.134f, 0.134f, 0.134f, 1.0f);
		
		// Frame backgrounds (inputs, sliders, etc.)
		colors[ImGuiCol_FrameBg]                = ImVec4(0.117f, 0.117f, 0.117f, 1.0f);
		colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
		colors[ImGuiCol_FrameBgActive]          = ImVec4(0.184f, 0.184f, 0.184f, 1.0f);
		
		// Tabs (active tab with teal underline feel)
		colors[ImGuiCol_Tab]                    = ImVec4(0.107f, 0.107f, 0.107f, 1.0f);
		colors[ImGuiCol_TabHovered]             = ImVec4(accent.x, accent.y, accent.z, 0.30f);
		colors[ImGuiCol_TabActive]              = ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f);
		colors[ImGuiCol_TabUnfocused]           = ImVec4(0.094f, 0.094f, 0.094f, 1.0f);
		colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.134f, 0.134f, 0.134f, 1.0f);
		
		// Title bar
		colors[ImGuiCol_TitleBg]                = ImVec4(0.084f, 0.084f, 0.084f, 1.0f);
		colors[ImGuiCol_TitleBgActive]          = ImVec4(0.107f, 0.107f, 0.107f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.084f, 0.084f, 0.084f, 1.0f);
		
		// Scrollbar
		colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.084f, 0.084f, 0.084f, 0.5f);
		colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.251f, 0.251f, 0.251f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.341f, 0.341f, 0.341f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.431f, 0.431f, 0.431f, 1.0f);
		
		// Checkmarks and sliders use teal accent
		colors[ImGuiCol_CheckMark]              = accent;
		colors[ImGuiCol_SliderGrab]             = accent;
		colors[ImGuiCol_SliderGrabActive]       = accentHover;
		
		// Resize grip
		colors[ImGuiCol_ResizeGrip]             = ImVec4(accent.x, accent.y, accent.z, 0.15f);
		colors[ImGuiCol_ResizeGripHovered]      = ImVec4(accent.x, accent.y, accent.z, 0.35f);
		colors[ImGuiCol_ResizeGripActive]       = ImVec4(accent.x, accent.y, accent.z, 0.55f);
		
		// Separator
		colors[ImGuiCol_Separator]              = separator;
		colors[ImGuiCol_SeparatorHovered]       = ImVec4(accent.x, accent.y, accent.z, 0.50f);
		colors[ImGuiCol_SeparatorActive]        = accent;
		
		// Docking
		colors[ImGuiCol_DockingPreview]         = ImVec4(accent.x, accent.y, accent.z, 0.35f);
		colors[ImGuiCol_DockingEmptyBg]         = bgVeryDark;
		
		// Tables
		colors[ImGuiCol_TableHeaderBg]          = bgHeader;
		colors[ImGuiCol_TableBorderStrong]      = border;
		colors[ImGuiCol_TableBorderLight]       = ImVec4(0.167f, 0.167f, 0.167f, 1.0f);
		colors[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
		
		// Plots
		colors[ImGuiCol_PlotLines]              = accent;
		colors[ImGuiCol_PlotLinesHovered]       = accentHover;
		colors[ImGuiCol_PlotHistogram]          = ImVec4(0.341f, 0.341f, 0.341f, 1.0f);
		colors[ImGuiCol_PlotHistogramHovered]   = accent;
		
		// Drag and drop
		colors[ImGuiCol_DragDropTarget]         = ImVec4(accent.x, accent.y, accent.z, 0.85f);
		
		// Navigation
		colors[ImGuiCol_NavHighlight]           = accent;
		colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
		
		// Modal
		colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
	}
}