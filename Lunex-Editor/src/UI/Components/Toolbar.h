/**
 * @file Toolbar.h
 * @brief Floating Toolbar Component
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// TOOLBAR COMPONENT
	// ============================================================================
	
	struct ToolbarStyle {
		Color backgroundColor = Color(0, 0, 0, 0);
		float buttonSize = 32.0f;
		float spacing = 8.0f;
		float padding = 32.0f;
		float buttonRounding = 6.0f;
	};
	
	/**
	 * @class Toolbar
	 * @brief Renders a floating toolbar with icon buttons
	 * 
	 * Features:
	 * - Floating window
	 * - Icon buttons with fallback text
	 * - Active state highlighting
	 * - Tooltips
	 */
	class Toolbar {
	public:
		Toolbar() = default;
		~Toolbar() = default;
		
		/**
		 * @brief Begin the toolbar
		 * @param id Unique identifier
		 * @param position Screen position
		 * @param buttonCount Number of buttons
		 * @return true if toolbar is visible
		 */
		bool Begin(const std::string& id,
				   const Position& position,
				   int buttonCount);
		
		/**
		 * @brief End the toolbar
		 */
		void End();
		
		/**
		 * @brief Render a toolbar button
		 * @param id Button identifier
		 * @param icon Button icon texture
		 * @param fallbackText Text to show if no icon
		 * @param isActive Whether button is in active state
		 * @param tooltip Tooltip text
		 * @return true if clicked
		 */
		bool Button(const std::string& id,
					Ref<Texture2D> icon,
					const char* fallbackText,
					bool isActive = false,
					const char* tooltip = nullptr);
		
		// Style configuration
		void SetStyle(const ToolbarStyle& style) { m_Style = style; }
		ToolbarStyle& GetStyle() { return m_Style; }
		const ToolbarStyle& GetStyle() const { return m_Style; }
		
	private:
		ToolbarStyle m_Style;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	bool BeginToolbar(const std::string& id,
					  const Position& position,
					  int buttonCount,
					  const ToolbarStyle& style = ToolbarStyle());
	
	void EndToolbar();
	
	bool ToolbarButton(const std::string& id,
					   Ref<Texture2D> icon,
					   const char* fallbackText,
					   bool isActive = false,
					   const char* tooltip = nullptr,
					   float size = 32.0f);

} // namespace Lunex::UI
