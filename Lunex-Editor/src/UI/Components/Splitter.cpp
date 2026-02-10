/**
 * @file Splitter.cpp
 * @brief Splitter Component Implementation
 */

#include "stpch.h"
#include "Splitter.h"

namespace Lunex::UI {

	bool Splitter::RenderVertical(const std::string& id, float& width,
								  float minWidth, float maxWidth, float height) {
		ScopedID scopedID(id);
		
		ScopedColor colors({
			{ImGuiCol_Button, m_Style.normalColor},
			{ImGuiCol_ButtonHovered, m_Style.hoverColor},
			{ImGuiCol_ButtonActive, m_Style.activeColor}
		});
		
		ImGui::Button(("##" + id).c_str(), ImVec2(m_Style.thickness, height));
		
		bool isDragging = ImGui::IsItemActive();
		
		if (isDragging) {
			width += ImGui::GetIO().MouseDelta.x;
			width = std::clamp(width, minWidth, maxWidth);
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		
		return isDragging;
	}
	
	bool Splitter::RenderHorizontal(const std::string& id, float& height,
									float minHeight, float maxHeight, float width) {
		ScopedID scopedID(id);
		
		ScopedColor colors({
			{ImGuiCol_Button, m_Style.normalColor},
			{ImGuiCol_ButtonHovered, m_Style.hoverColor},
			{ImGuiCol_ButtonActive, m_Style.activeColor}
		});
		
		ImGui::Button(("##" + id).c_str(), ImVec2(width, m_Style.thickness));
		
		bool isDragging = ImGui::IsItemActive();
		
		if (isDragging) {
			height += ImGui::GetIO().MouseDelta.y;
			height = std::clamp(height, minHeight, maxHeight);
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
		
		return isDragging;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	bool VerticalSplitter(const std::string& id, float& width,
						  float minWidth, float maxWidth, float height) {
		Splitter splitter;
		return splitter.RenderVertical(id, width, minWidth, maxWidth, height);
	}
	
	bool HorizontalSplitter(const std::string& id, float& height,
							float minHeight, float maxHeight, float width) {
		Splitter splitter;
		return splitter.RenderHorizontal(id, height, minHeight, maxHeight, width);
	}

} // namespace Lunex::UI
