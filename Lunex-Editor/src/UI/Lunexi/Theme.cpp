#include "stpch.h"
#include "Theme.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Lunex::UI {

	namespace {
		std::filesystem::path PreferenceFile() {
			char* appData = nullptr;
			size_t len = 0;
			std::filesystem::path base = std::filesystem::current_path();
			if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) {
				base = std::filesystem::path(appData) / "Lunex";
				free(appData);
			}
			return base / "EditorTheme.txt";
		}

		Theme MakeLunexDark() {
			Theme t;
			t.Name = "Lunex Dark";
			t.Tokens = ThemeTokens{};   // defaults are the Lunex Dark palette
			t.Tokens.FrameRounding = 3.0f;
			return t;
		}

		Theme MakeGraphite() {
			Theme t;
			t.Name = "Lunex Graphite";
			auto& k = t.Tokens;
			k.BgDark = Color::FromHex(0x1E1E1E);
			k.BgMedium = Color::FromHex(0x252526);
			k.BgLight = Color::FromHex(0x2F2F30);
			k.BgCard = Color::FromHex(0x2A2A2B);
			k.BgHover = Color::FromHex(0x3A3A3C);
			k.Border = Color::FromHex(0x141414);
			k.BorderLight = Color::FromHex(0x444446);
			k.TextPrimary = Color(0.92f, 0.92f, 0.92f, 1.0f);
			k.TextSecondary = Color(0.64f, 0.64f, 0.64f, 1.0f);
			k.FrameRounding = 4.0f;
			k.TabRounding = 4.0f;
			k.GrabRounding = 4.0f;
			return t;
		}

		Theme MakeMidnight() {
			Theme t;
			t.Name = "Midnight";
			auto& k = t.Tokens;
			k.Primary = Color::FromHex(0x3FB6D9);
			k.PrimaryHover = Color::FromHex(0x62C8E6);
			k.PrimaryActive = Color::FromHex(0x2A93B3);
			k.BgDark = Color::FromHex(0x0F1218);
			k.BgMedium = Color::FromHex(0x141821);
			k.BgLight = Color::FromHex(0x1C2230);
			k.BgCard = Color::FromHex(0x181D28);
			k.BgHover = Color::FromHex(0x252D3D);
			k.Border = Color::FromHex(0x090B10);
			k.BorderLight = Color::FromHex(0x2C3446);
			k.BorderFocus = Color(0.247f, 0.714f, 0.851f, 0.5f);
			k.Selected = Color(0.247f, 0.714f, 0.851f, 0.18f);
			k.SelectedBorder = k.Primary;
			k.FrameRounding = 4.0f;
			return t;
		}

		Theme MakeClassicBlue() {
			Theme t;
			t.Name = "Classic Blue";
			auto& k = t.Tokens;
			k.Primary = Color::FromHex(0x4A90D9);
			k.PrimaryHover = Color::FromHex(0x64A4E6);
			k.PrimaryActive = Color::FromHex(0x3576BD);
			k.BgDark = Color::FromHex(0x16191F);
			k.BgMedium = Color::FromHex(0x1B1F26);
			k.BgLight = Color::FromHex(0x252A33);
			k.BgCard = Color::FromHex(0x20242C);
			k.BgHover = Color::FromHex(0x2E3440);
			k.BorderLight = Color::FromHex(0x343B48);
			k.BorderFocus = Color(0.290f, 0.565f, 0.851f, 0.5f);
			k.Selected = Color(0.290f, 0.565f, 0.851f, 0.20f);
			k.SelectedBorder = k.Primary;
			return t;
		}

		size_t s_Current = 0;
	}

	const ThemeTokens& CurrentTokens() {
		return Theme::GetPresets()[s_Current].Tokens;
	}

	const std::vector<Theme>& Theme::GetPresets() {
		static const std::vector<Theme> presets = { MakeLunexDark(), MakeGraphite(), MakeMidnight(), MakeClassicBlue() };
		return presets;
	}

	const Theme& Theme::GetCurrent() { return GetPresets()[s_Current]; }

	bool Theme::SetCurrent(const std::string& name, bool persist) {
		const auto& presets = GetPresets();
		for (size_t i = 0; i < presets.size(); i++) {
			if (presets[i].Name != name) continue;
			s_Current = i;
			presets[i].ApplyToImGui();
			if (persist) {
				std::error_code ec;
				auto file = PreferenceFile();
				std::filesystem::create_directories(file.parent_path(), ec);
				std::ofstream(file, std::ios::trunc) << name;
			}
			return true;
		}
		return false;
	}

	void Theme::LoadUserPreference() {
		std::string name;
		std::ifstream in(PreferenceFile());
		if (in) std::getline(in, name);
		if (name.empty() || !SetCurrent(name, false)) SetCurrent(GetPresets()[0].Name, false);
	}

	void Theme::ApplyToImGui() const {
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* c = style.Colors;
		const auto& k = Tokens;
		auto v = [](const Color& col, float alpha = -1.0f) { ImVec4 r = col.ToImVec4(); if (alpha >= 0.0f) r.w = alpha; return r; };
		auto mix = [](const Color& a, const Color& b, float t) {
			return ImVec4(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f);
		};

		// Metrics
		style.WindowRounding = k.WindowRounding;
		style.ChildRounding = k.FrameRounding * 0.5f;
		style.FrameRounding = k.FrameRounding;
		style.PopupRounding = k.FrameRounding;
		style.GrabRounding = k.GrabRounding;
		style.TabRounding = k.TabRounding;
		style.ScrollbarRounding = 9.0f;
		style.FramePadding = k.FramePadding;
		style.ItemSpacing = k.ItemSpacing;
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		style.TabBarOverlineSize = 2.0f;   // accent line over the selected tab (Unreal-like)
		style.SeparatorTextBorderSize = 1.0f;

		// Backgrounds
		c[ImGuiCol_WindowBg] = v(k.BgMedium);
		c[ImGuiCol_ChildBg] = v(k.BgDark);
		c[ImGuiCol_PopupBg] = v(k.BgMedium.Lighter(0.01f), 0.98f);
		c[ImGuiCol_MenuBarBg] = v(k.BgDark.Lighter(0.02f));
		c[ImGuiCol_Border] = v(k.Border);
		c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

		// Text
		c[ImGuiCol_Text] = v(k.TextPrimary);
		c[ImGuiCol_TextDisabled] = v(k.TextMuted);
		c[ImGuiCol_TextSelectedBg] = v(k.Primary, 0.30f);

		// Interactive
		c[ImGuiCol_Header] = v(k.BgLight);
		c[ImGuiCol_HeaderHovered] = v(k.BgHover);
		c[ImGuiCol_HeaderActive] = v(k.Primary, 0.28f);
		c[ImGuiCol_Button] = v(k.BgLight);
		c[ImGuiCol_ButtonHovered] = v(k.BgHover);
		c[ImGuiCol_ButtonActive] = v(k.BgLight.Darker(0.03f));
		c[ImGuiCol_FrameBg] = v(k.BgDark.Lighter(0.015f));
		c[ImGuiCol_FrameBgHovered] = v(k.BgLight);
		c[ImGuiCol_FrameBgActive] = v(k.BgHover);
		c[ImGuiCol_CheckMark] = v(k.Primary);
		c[ImGuiCol_SliderGrab] = v(k.Primary);
		c[ImGuiCol_SliderGrabActive] = v(k.PrimaryHover);

		// Tabs: neutral, with the accent overline marking the selected one
		c[ImGuiCol_Tab] = v(k.BgDark);
		c[ImGuiCol_TabHovered] = v(k.BgHover);
		c[ImGuiCol_TabSelected] = v(k.BgMedium);
		c[ImGuiCol_TabSelectedOverline] = v(k.Primary);
		c[ImGuiCol_TabDimmed] = v(k.BgDark.Darker(0.01f));
		c[ImGuiCol_TabDimmedSelected] = v(k.BgMedium.Darker(0.01f));
		c[ImGuiCol_TabDimmedSelectedOverline] = v(k.BorderLight);

		// Title bars
		c[ImGuiCol_TitleBg] = v(k.BgDark);
		c[ImGuiCol_TitleBgActive] = v(k.BgDark.Lighter(0.02f));
		c[ImGuiCol_TitleBgCollapsed] = v(k.BgDark);

		// Scrollbars
		c[ImGuiCol_ScrollbarBg] = v(k.BgDark, 0.5f);
		c[ImGuiCol_ScrollbarGrab] = v(k.BorderLight);
		c[ImGuiCol_ScrollbarGrabHovered] = v(k.BorderLight.Lighter(0.08f));
		c[ImGuiCol_ScrollbarGrabActive] = v(k.BorderLight.Lighter(0.16f));

		// Separators / resize
		c[ImGuiCol_Separator] = v(k.BorderLight, 0.6f);
		c[ImGuiCol_SeparatorHovered] = v(k.Primary, 0.5f);
		c[ImGuiCol_SeparatorActive] = v(k.Primary);
		c[ImGuiCol_ResizeGrip] = v(k.Primary, 0.12f);
		c[ImGuiCol_ResizeGripHovered] = v(k.Primary, 0.35f);
		c[ImGuiCol_ResizeGripActive] = v(k.Primary, 0.55f);

		// Docking, tables, plots, misc
		c[ImGuiCol_DockingPreview] = v(k.Primary, 0.35f);
		c[ImGuiCol_DockingEmptyBg] = v(k.BgDark);
		c[ImGuiCol_TableHeaderBg] = v(k.BgDark.Lighter(0.02f));
		c[ImGuiCol_TableBorderStrong] = v(k.Border);
		c[ImGuiCol_TableBorderLight] = v(k.BgLight);
		c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
		c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);
		c[ImGuiCol_PlotLines] = v(k.Primary);
		c[ImGuiCol_PlotLinesHovered] = v(k.PrimaryHover);
		c[ImGuiCol_PlotHistogram] = mix(k.BorderLight, k.Primary, 0.3f);
		c[ImGuiCol_PlotHistogramHovered] = v(k.Primary);
		c[ImGuiCol_DragDropTarget] = v(k.Primary, 0.85f);
		c[ImGuiCol_NavCursor] = v(k.Primary);
		c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
		c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.6f);
		c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.7f);
	}

} // namespace Lunex::UI
