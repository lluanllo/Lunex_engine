/**
 * @file ToolbarButton.cpp
 * @brief Toolbar Button Component Implementation
 */

#include "stpch.h"
#include "ToolbarButton.h"
#include <imgui.h>

namespace Lunex::UI {

	bool ToolbarButton(const ToolbarButtonProps& props) {
		bool clicked = false;
		
		ScopedID buttonID(props.Id);
		
		// Apply selected state colors
		if (props.IsSelected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ToolbarButtonColors::ButtonSelected().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToolbarButtonColors::ButtonSelectedHover().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolbarButtonColors::ButtonSelectedHover().ToImVec4());
		} else {
			ImGui::PushStyleColor(ImGuiCol_Button, ToolbarButtonColors::ButtonBg().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToolbarButtonColors::ButtonHover().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolbarButtonColors::ButtonActive().ToImVec4());
		}
		
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ToolbarButtonSizing::ButtonRounding);
		
		// Determine tint based on enabled state
		Color tint = props.IsEnabled ? props.Tint : props.Tint.WithAlpha(0.4f);
		
		// Render icon button or text fallback
		if (props.Icon && props.Icon->GetRendererID() != 0) {
			clicked = ImGui::ImageButton(
				("##" + props.Id).c_str(),
				(ImTextureID)(intptr_t)props.Icon->GetRendererID(),
				ImVec2(props.Size, props.Size),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0),
				tint.ToImVec4()
			);
		} else if (props.FallbackText) {
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			clicked = ImGui::Button(props.FallbackText, ImVec2(props.Size, props.Size));
			ImGui::PopStyleVar();
		}
		
		// Only trigger click if enabled
		clicked = clicked && props.IsEnabled;
		
		// Tooltip
		if (ImGui::IsItemHovered() && (props.TooltipTitle || props.TooltipDescription)) {
			ToolbarTooltip(props.TooltipTitle, props.TooltipDescription);
		}
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		
		return clicked;
	}

	void ToolbarTooltip(const char* title, const char* description) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
		ImGui::BeginTooltip();
		
		if (title) {
			ImGui::PushStyleColor(ImGuiCol_Text, ToolbarButtonColors::TooltipTitle().ToImVec4());
			ImGui::Text("%s", title);
			ImGui::PopStyleColor();
		}
		
		if (description) {
			ImGui::PushStyleColor(ImGuiCol_Text, ToolbarButtonColors::TooltipDesc().ToImVec4());
			ImGui::Text("%s", description);
			ImGui::PopStyleColor();
		}
		
		ImGui::EndTooltip();
		ImGui::PopStyleVar();
	}

} // namespace Lunex::UI
