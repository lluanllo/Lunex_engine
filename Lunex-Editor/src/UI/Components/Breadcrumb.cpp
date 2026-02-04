/**
 * @file Breadcrumb.cpp
 * @brief Breadcrumb Navigation Component Implementation
 */

#include "stpch.h"
#include "Breadcrumb.h"
#include "../UILayout.h"

namespace Lunex::UI {

	int Breadcrumb::Render(const std::string& id, const std::vector<BreadcrumbItem>& items) {
		ScopedID scopedID(id);
		
		int clickedIndex = -1;
		
		for (size_t i = 0; i < items.size(); i++) {
			ScopedID itemID((int)i);
			
			// Separator
			if (i > 0) {
				RenderSeparator();
			}
			
			// Item
			bool isLast = (i == items.size() - 1);
			if (RenderItem(items[i], isLast)) {
				clickedIndex = (int)i;
			}
			
			if (!isLast) {
				SameLine();
			}
		}
		
		return clickedIndex;
	}
	
	void Breadcrumb::RenderSeparator() {
		ScopedColor sepColor(ImGuiCol_Text, m_Style.separatorColor);
		ImGui::Text("%s", m_Style.separator);
		SameLine();
	}
	
	bool Breadcrumb::RenderItem(const BreadcrumbItem& item, bool isLast) {
		Color textColor = isLast ? m_Style.activeTextColor : m_Style.textColor;
		
		ScopedColor tc(ImGuiCol_Text, textColor);
		
		float textWidth = ImGui::CalcTextSize(item.label.c_str()).x;
		return ImGui::Selectable(item.label.c_str(), false, 
			ImGuiSelectableFlags_DontClosePopups, 
			ImVec2(textWidth, 0));
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	int RenderBreadcrumb(const std::string& id, const std::vector<BreadcrumbItem>& items) {
		Breadcrumb breadcrumb;
		return breadcrumb.Render(id, items);
	}

} // namespace Lunex::UI
