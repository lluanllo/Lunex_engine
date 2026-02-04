/**
 * @file Toolbar.cpp
 * @brief Floating Toolbar Component Implementation
 */

#include "stpch.h"
#include "Toolbar.h"

namespace Lunex::UI {

	bool Toolbar::Begin(const std::string& id, const Position& position, int buttonCount) {
		float totalWidth = (m_Style.buttonSize * buttonCount) + (m_Style.spacing * (buttonCount - 1)) + (m_Style.padding * 2);
		float totalHeight = m_Style.buttonSize + (m_Style.padding * 2);
		
		ImGui::SetNextWindowPos(ToImVec2(position), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m_Style.padding, m_Style.padding));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_Style.spacing, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ToImVec4(m_Style.backgroundColor));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
								 ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoScrollbar |
								 ImGuiWindowFlags_NoScrollWithMouse |
								 ImGuiWindowFlags_NoSavedSettings |
								 ImGuiWindowFlags_NoFocusOnAppearing |
								 ImGuiWindowFlags_NoDocking;
		
		return ImGui::Begin(id.c_str(), nullptr, flags);
	}
	
	void Toolbar::End() {
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}
	
	bool Toolbar::Button(const std::string& id, Ref<Texture2D> icon, const char* fallbackText,
						 bool isActive, const char* tooltip) {
		ScopedID scopedID(id);
		
		Color btnColor = isActive ? Colors::Primary() : Color(0.15f, 0.15f, 0.15f, 0.7f);
		Color hoverColor = isActive ? Colors::PrimaryHover() : Color(0.25f, 0.25f, 0.25f, 0.85f);
		
		ScopedColor colors({
			{ImGuiCol_Button, btnColor},
			{ImGuiCol_ButtonHovered, hoverColor},
			{ImGuiCol_ButtonActive, Color(0.30f, 0.30f, 0.30f, 0.9f)}
		});
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, m_Style.buttonRounding);
		
		bool clicked = false;
		
		if (icon) {
			clicked = ImGui::ImageButton(id.c_str(),
				(ImTextureID)(intptr_t)icon->GetRendererID(),
				ImVec2(m_Style.buttonSize, m_Style.buttonSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0),
				ImVec4(1, 1, 1, 1));
		} else {
			ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			clicked = ImGui::Button(fallbackText, ImVec2(m_Style.buttonSize, m_Style.buttonSize));
		}
		
		if (tooltip && ImGui::IsItemHovered()) {
			ScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
			ImGui::BeginTooltip();
			ScopedColor textColor(ImGuiCol_Text, Colors::TextPrimary());
			ImGui::Text("%s", tooltip);
			ImGui::EndTooltip();
		}
		
		return clicked;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	static Toolbar s_ActiveToolbar;
	
	bool BeginToolbar(const std::string& id, const Position& position, int buttonCount,
					  const ToolbarStyle& style) {
		s_ActiveToolbar.SetStyle(style);
		return s_ActiveToolbar.Begin(id, position, buttonCount);
	}
	
	void EndToolbar() {
		s_ActiveToolbar.End();
	}
	
	bool ToolbarButton(const std::string& id, Ref<Texture2D> icon, const char* fallbackText,
					   bool isActive, const char* tooltip, float size) {
		ToolbarStyle style = s_ActiveToolbar.GetStyle();
		style.buttonSize = size;
		s_ActiveToolbar.SetStyle(style);
		return s_ActiveToolbar.Button(id, icon, fallbackText, isActive, tooltip);
	}

} // namespace Lunex::UI
