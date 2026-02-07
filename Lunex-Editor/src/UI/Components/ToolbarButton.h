/**
 * @file ToolbarButton.h
 * @brief Toolbar Button Component - Reusable icon button for toolbars
 * 
 * Features:
 * - Icon with optional text fallback
 * - Active state with highlight color
 * - Styled tooltip with title and description
 * - Translucent background style
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <string>

namespace Lunex::UI {

	// ============================================================================
	// TOOLBAR BUTTON COLORS
	// ============================================================================
	
	namespace ToolbarButtonColors {
		// Button colors (translucent with subtle effects)
		inline Color ButtonBg()          { return Color(0.15f, 0.15f, 0.15f, 0.70f); }
		inline Color ButtonHover()       { return Color(0.25f, 0.25f, 0.25f, 0.85f); }
		inline Color ButtonActive()      { return Color(0.30f, 0.30f, 0.30f, 0.90f); }
		inline Color ButtonSelected()    { return Color(0.26f, 0.59f, 0.98f, 0.90f); }
		inline Color ButtonSelectedHover() { return Color(0.36f, 0.69f, 1.0f, 1.0f); }
		
		// Tooltip colors
		inline Color TooltipTitle()      { return Color(0.90f, 0.90f, 0.90f, 1.0f); }
		inline Color TooltipDesc()       { return Color(0.70f, 0.70f, 0.70f, 1.0f); }
	}
	
	// Sizing constants
	namespace ToolbarButtonSizing {
		constexpr float DefaultButtonSize = 28.0f;
		constexpr float LargeButtonSize = 32.0f;
		constexpr float ButtonRounding = 6.0f;
	}

	// ============================================================================
	// TOOLBAR BUTTON COMPONENT
	// ============================================================================

	struct ToolbarButtonProps {
		std::string Id;
		Ref<Texture2D> Icon = nullptr;
		const char* FallbackText = nullptr;      // Used when icon is null
		float Size = ToolbarButtonSizing::DefaultButtonSize;
		bool IsSelected = false;
		bool IsEnabled = true;
		const char* TooltipTitle = nullptr;
		const char* TooltipDescription = nullptr;
		Color Tint = Colors::TextPrimary();
	};

	/**
	 * @brief Render a toolbar button with icon and styled tooltip
	 * @return true if clicked
	 */
	bool ToolbarButton(const ToolbarButtonProps& props);

	/**
	 * @brief Render styled tooltip with title and description
	 */
	void ToolbarTooltip(const char* title, const char* description = nullptr);

} // namespace Lunex::UI
