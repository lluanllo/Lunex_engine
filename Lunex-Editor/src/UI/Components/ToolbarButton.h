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
		inline Color ButtonBg()          { return Color(0.12f, 0.12f, 0.12f, 0.70f); }
		inline Color ButtonHover()       { return Color(0.22f, 0.22f, 0.22f, 0.85f); }
		inline Color ButtonActive()      { return Color(0.08f, 0.08f, 0.08f, 0.90f); }
		inline Color ButtonSelected()    { return Color(0.91f, 0.57f, 0.18f, 0.90f); }
		inline Color ButtonSelectedHover() { return Color(0.94f, 0.63f, 0.29f, 1.0f); }
		
		// Tooltip colors
		inline Color TooltipTitle()      { return Color(0.92f, 0.92f, 0.92f, 1.0f); }
		inline Color TooltipDesc()       { return Color(0.62f, 0.62f, 0.62f, 1.0f); }
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
