/**
 * @file UIText.cpp
 * @brief Text UI Components Implementation
 */

#include "stpch.h"
#include "UIText.h"

namespace Lunex::UI {

	void TextStyled(const std::string& text, TextVariant variant) {
		Color color;
		switch (variant) {
			case TextVariant::Primary:   color = Colors::TextPrimary(); break;
			case TextVariant::Secondary: color = Colors::TextSecondary(); break;
			case TextVariant::Muted:     color = Colors::TextMuted(); break;
			case TextVariant::Success:   color = Colors::Success(); break;
			case TextVariant::Warning:   color = Colors::Warning(); break;
			case TextVariant::Danger:    color = Colors::Danger(); break;
			default:                     color = Colors::TextPrimary(); break;
		}
		
		ScopedColor col(ImGuiCol_Text, color);
		ImGui::TextUnformatted(text.c_str());
	}
	
	void Text(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}
	
	void TextColored(const Color& color, const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ImGui::TextColoredV(ToImVec4(color), fmt, args);
		va_end(args);
	}
	
	void TextWrapped(const std::string& text, TextVariant variant) {
		Color color;
		switch (variant) {
			case TextVariant::Primary:   color = Colors::TextPrimary(); break;
			case TextVariant::Secondary: color = Colors::TextSecondary(); break;
			case TextVariant::Muted:     color = Colors::TextMuted(); break;
			case TextVariant::Success:   color = Colors::Success(); break;
			case TextVariant::Warning:   color = Colors::Warning(); break;
			case TextVariant::Danger:    color = Colors::Danger(); break;
			default:                     color = Colors::TextPrimary(); break;
		}
		
		ScopedColor col(ImGuiCol_Text, color);
		ImGui::TextWrapped("%s", text.c_str());
	}
	
	void Label(const std::string& text, const char* tooltip) {
		ImGui::AlignTextToFramePadding();
		ScopedColor col(ImGuiCol_Text, Colors::TextSecondary());
		ImGui::TextUnformatted(text.c_str());
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
	}
	
	void Heading(const std::string& text, int level) {
		float scale = 1.0f;
		switch (level) {
			case 1: scale = 1.4f; break;
			case 2: scale = 1.2f; break;
			case 3: scale = 1.1f; break;
			default: scale = 1.0f; break;
		}
		
		ScopedColor col(ImGuiCol_Text, Colors::TextPrimary());
		ImGui::SetWindowFontScale(scale);
		ImGui::TextUnformatted(text.c_str());
		ImGui::SetWindowFontScale(1.0f);
	}
	
	void Badge(const std::string& text, const Color& bgColor, const Color& textColor) {
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
		ImVec2 padding(6.0f, 2.0f);
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		ImVec2 min = pos;
		ImVec2 max = ImVec2(pos.x + textSize.x + padding.x * 2, pos.y + textSize.y + padding.y * 2);
		
		drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(bgColor)), 3.0f);
		
		ImGui::SetCursorScreenPos(ImVec2(pos.x + padding.x, pos.y + padding.y));
		ScopedColor col(ImGuiCol_Text, textColor);
		ImGui::TextUnformatted(text.c_str());
		
		ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y + 2.0f));
	}
	
	void BulletText(const std::string& text) {
		ImGui::BulletText("%s", text.c_str());
	}

} // namespace Lunex::UI
