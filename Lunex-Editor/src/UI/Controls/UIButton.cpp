/**
 * @file UIButton.cpp
 * @brief Button UI Components Implementation
 */

#include "stpch.h"
#include "UIButton.h"

namespace Lunex::UI {

	static void ApplyButtonStyle(ButtonVariant variant) {
		switch (variant) {
			case ButtonVariant::Primary:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Primary()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::PrimaryHover()));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Colors::PrimaryActive()));
				break;
			case ButtonVariant::Success:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Success()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.90f, 0.51f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.70f, 0.40f, 1.0f));
				break;
			case ButtonVariant::Warning:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.55f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.65f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.45f, 0.10f, 1.0f));
				break;
			case ButtonVariant::Danger:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Danger()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.40f, 0.35f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.25f, 0.20f, 1.0f));
				break;
			case ButtonVariant::Ghost:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 0.50f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.20f, 0.20f, 0.70f));
				break;
			case ButtonVariant::Outline:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Color(Colors::Primary().r, Colors::Primary().g, Colors::Primary().b, 0.15f)));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Color(Colors::Primary().r, Colors::Primary().g, Colors::Primary().b, 0.25f)));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(Colors::Primary()));
				break;
			default:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
				break;
		}
	}
	
	static void PopButtonStyle(ButtonVariant variant) {
		if (variant == ButtonVariant::Outline) {
			ImGui::PopStyleColor(4);
			ImGui::PopStyleVar();
		} else {
			ImGui::PopStyleColor(3);
		}
	}
	
	static ImVec2 GetButtonSize(ButtonSize size, const Size& customSize) {
		if (customSize.x > 0 || customSize.y > 0) {
			return ToImVec2(customSize);
		}
		
		switch (size) {
			case ButtonSize::Small:  return ImVec2(0, 22.0f);
			case ButtonSize::Large:  return ImVec2(0, SpacingValues::ButtonHeightLG);
			default:                 return ImVec2(0, SpacingValues::ButtonHeight);
		}
	}
	
	bool Button(const std::string& label, ButtonVariant variant, ButtonSize size, const Size& customSize) {
		ApplyButtonStyle(variant);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		
		ImVec2 btnSize = GetButtonSize(size, customSize);
		bool clicked = ImGui::Button(label.c_str(), btnSize);
		
		PopButtonStyle(variant);
		return clicked;
	}
	
	bool ButtonBlock(const std::string& label, ButtonVariant variant, float height) {
		return Button(label, variant, ButtonSize::Medium, Size(-1, height));
	}
	
	bool IconButton(const std::string& id, Ref<Texture2D> icon, float size, const char* tooltip, const Color& tint) {
		bool clicked = false;
		
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		ScopedColor colors({
			{ImGuiCol_Button, Color(0.14f, 0.14f, 0.14f, 0.70f)},
			{ImGuiCol_ButtonHovered, Color(0.22f, 0.22f, 0.22f, 0.85f)},
			{ImGuiCol_ButtonActive, Color(0.10f, 0.10f, 0.10f, 0.90f)}
		});
		
		if (icon) {
			clicked = ImGui::ImageButton(id.c_str(),
				(ImTextureID)(intptr_t)icon->GetRendererID(),
				ImVec2(size, size),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0),
				ToImVec4(tint));
		} else {
			clicked = ImGui::Button("##btn", ImVec2(size + 8, size + 8));
		}
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return clicked;
	}
	
	bool IconButton(const std::string& id, Ref<Texture2D> icon, const char* fallbackText, float size, const char* tooltip) {
		if (icon) {
			return IconButton(id, icon, size, tooltip);
		}
		
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		
		bool clicked = ImGui::Button(fallbackText, ImVec2(size + 8, size + 8));
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return clicked;
	}
	
	bool ButtonWithIcon(const std::string& label, const char* icon, ButtonVariant variant) {
		std::string fullLabel = std::string(icon) + " " + label;
		return Button(fullLabel, variant);
	}

} // namespace Lunex::UI
