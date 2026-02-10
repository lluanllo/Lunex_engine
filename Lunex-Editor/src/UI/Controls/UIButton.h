/**
 * @file UIButton.h
 * @brief Button UI Components
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// BUTTON COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Standard button with variants
	 * @return true if clicked
	 */
	bool Button(const std::string& label, 
				ButtonVariant variant = ButtonVariant::Default,
				ButtonSize size = ButtonSize::Medium,
				const Size& customSize = Size(0, 0));
	
	/**
	 * @brief Button that fills available width
	 */
	bool ButtonBlock(const std::string& label,
					 ButtonVariant variant = ButtonVariant::Default,
					 float height = SpacingValues::ButtonHeight);
	
	/**
	 * @brief Icon button (square, typically for toolbars)
	 */
	bool IconButton(const std::string& id, 
					Ref<Texture2D> icon,
					float size = SpacingValues::IconLG,
					const char* tooltip = nullptr,
					const Color& tint = Colors::TextPrimary());
	
	/**
	 * @brief Icon button with text fallback
	 */
	bool IconButton(const std::string& id,
					Ref<Texture2D> icon,
					const char* fallbackText,
					float size = SpacingValues::IconLG,
					const char* tooltip = nullptr);
	
	/**
	 * @brief Button with icon and text
	 */
	bool ButtonWithIcon(const std::string& label,
						const char* icon,
						ButtonVariant variant = ButtonVariant::Default);

} // namespace Lunex::UI
