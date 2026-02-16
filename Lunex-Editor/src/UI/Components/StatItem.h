/**
 * @file StatItem.h
 * @brief Stat Item Component - Display statistics with label and value
 */

#pragma once

#include "../UICore.h"
#include <string>

namespace Lunex::UI {

	// ============================================================================
	// STAT DISPLAY STYLES
	// ============================================================================
	
	namespace StatStyle {
		inline Color LabelColor()    { return Colors::TextSecondary(); }
		inline Color ValueColor()    { return Colors::TextPrimary(); }
		inline Color HeaderColor()   { return Colors::Primary(); }
		inline Color SectionBg()     { return Color(0.07f, 0.09f, 0.11f, 0.80f); }
	}

	// ============================================================================
	// STAT ITEM COMPONENTS
	// ============================================================================

	/**
	 * @brief Display a single stat with label and value
	 */
	void StatItem(const char* label, const char* value);
	void StatItem(const char* label, int value);
	void StatItem(const char* label, float value, const char* format = "%.2f");
	void StatItem(const char* label, uint32_t value);
	
	/**
	 * @brief Display a stat header (section title)
	 */
	void StatHeader(const char* title);

	/**
	 * @brief Begin a stat section with background
	 */
	bool BeginStatSection(const char* title);
	void EndStatSection();

} // namespace Lunex::UI
