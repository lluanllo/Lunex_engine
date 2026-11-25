/**
 * @file ToolbarButton.cpp
 * @brief Toolbar Button Component Implementation
 */

#include "stpch.h"
#include "ToolbarButton.h"
#include <imgui.h>
#include <cmath>

namespace Lunex::UI {

	bool ToolbarButton(const ToolbarButtonProps& props) {
		ScopedID buttonID(props.Id);

		// Every button is exactly Size x Size whether it has an icon or not (ImageButton
		// adds frame padding, text buttons don't: mixed toolbars used to look uneven)
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const ImVec2 size(props.Size, props.Size);
		bool clicked = ImGui::InvisibleButton("##tb", size);
		const bool hovered = ImGui::IsItemHovered();
		const bool held = ImGui::IsItemActive();

		Color bg = props.IsSelected
			? (hovered ? ToolbarButtonColors::ButtonSelectedHover() : ToolbarButtonColors::ButtonSelected())
			: (held ? ToolbarButtonColors::ButtonActive() : hovered ? ToolbarButtonColors::ButtonHover() : ToolbarButtonColors::ButtonBg());
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 q(p.x + size.x, p.y + size.y);
		dl->AddRectFilled(p, q, ImGui::GetColorU32(bg.ToImVec4()), ToolbarButtonSizing::ButtonRounding);

		const Color tint = props.IsEnabled ? props.Tint : props.Tint.WithAlpha(0.4f);
		if (props.Icon && props.Icon->GetRendererID() != 0) {
			const float pad = std::round(props.Size * 0.18f);
			dl->AddImage((ImTextureID)(intptr_t)props.Icon->GetRendererID(), ImVec2(p.x + pad, p.y + pad), ImVec2(q.x - pad, q.y - pad),
				ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(tint.ToImVec4()));
		}
		else if (props.FallbackText) {
			const ImVec2 ts = ImGui::CalcTextSize(props.FallbackText);
			dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), ImGui::GetColorU32(tint.ToImVec4()), props.FallbackText);
		}

		clicked = clicked && props.IsEnabled;
		if (hovered && (props.TooltipTitle || props.TooltipDescription))
			ToolbarTooltip(props.TooltipTitle, props.TooltipDescription);
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
