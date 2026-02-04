/**
 * @file SearchBar.cpp
 * @brief Search Bar Component Implementation
 */

#include "stpch.h"
#include "SearchBar.h"

namespace Lunex::UI {

	bool SearchBar::Render(const std::string& id, char* buffer, size_t bufferSize,
						   const char* placeholder) {
		ScopedID scopedID(id);
		
		ScopedColor colors({
			{ImGuiCol_FrameBg, m_Style.backgroundColor},
			{ImGuiCol_FrameBgHovered, m_Style.hoverColor},
			{ImGuiCol_FrameBgActive, m_Style.focusColor}
		});
		
		ImGui::SetNextItemWidth(m_Style.width);
		
		std::string fullPlaceholder = std::string(m_Style.icon) + " " + std::string(placeholder);
		return ImGui::InputTextWithHint("##search", fullPlaceholder.c_str(), buffer, bufferSize);
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	bool RenderSearchBar(const std::string& id, char* buffer, size_t bufferSize,
						 const char* placeholder, float width) {
		SearchBar searchBar;
		searchBar.SetWidth(width);
		return searchBar.Render(id, buffer, bufferSize, placeholder);
	}

} // namespace Lunex::UI
