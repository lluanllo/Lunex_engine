/**
 * @file UIText.h
 * @brief Text UI Components
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// TEXT COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Display text with styling options
	 */
	void TextStyled(const std::string& text, TextVariant variant = TextVariant::Default);
	
	/**
	 * @brief Display text (variadic)
	 */
	void Text(const char* fmt, ...);
	
	/**
	 * @brief Text with specific color
	 */
	void TextColored(const Color& color, const char* fmt, ...);
	
	/**
	 * @brief Wrapped text with styling
	 */
	void TextWrapped(const std::string& text, TextVariant variant = TextVariant::Default);
	
	/**
	 * @brief Label with optional tooltip
	 */
	void Label(const std::string& text, const char* tooltip = nullptr);
	
	/**
	 * @brief Heading with level (1-3)
	 */
	void Heading(const std::string& text, int level = 1);
	
	/**
	 * @brief Badge with background and text color
	 */
	void Badge(const std::string& text, const Color& bgColor, const Color& textColor = Colors::TextPrimary());
	
	/**
	 * @brief Bullet text
	 */
	void BulletText(const std::string& text);

} // namespace Lunex::UI
