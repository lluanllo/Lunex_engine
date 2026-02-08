/**
 * @file SearchBar.h
 * @brief Search Bar Component
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// SEARCH BAR COMPONENT
	// ============================================================================
	
	struct SearchBarStyle {
		float width = 200.0f;
		Color backgroundColor = Color::FromHex(0x141414);
		Color hoverColor = Color::FromHex(0x1E1E1E);
		Color focusColor = Color(0.91f, 0.57f, 0.18f, 0.40f);
		const char* icon = "??";
	};
	
	/**
	 * @class SearchBar
	 * @brief Renders a search input with icon
	 * 
	 * Features:
	 * - Search icon prefix
	 * - Placeholder text
	 * - Configurable width
	 */
	class SearchBar {
	public:
		SearchBar() = default;
		~SearchBar() = default;
		
		/**
		 * @brief Render the search bar
		 * @param id Unique identifier
		 * @param buffer Text buffer
		 * @param bufferSize Buffer size
		 * @param placeholder Placeholder text
		 * @return true if text changed
		 */
		bool Render(const std::string& id,
					char* buffer,
					size_t bufferSize,
					const char* placeholder = "Search...");
		
		// Style configuration
		void SetStyle(const SearchBarStyle& style) { m_Style = style; }
		SearchBarStyle& GetStyle() { return m_Style; }
		const SearchBarStyle& GetStyle() const { return m_Style; }
		
		void SetWidth(float width) { m_Style.width = width; }
		
	private:
		SearchBarStyle m_Style;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	bool RenderSearchBar(const std::string& id,
						 char* buffer,
						 size_t bufferSize,
						 const char* placeholder = "Search...",
						 float width = 200.0f);

} // namespace Lunex::UI
