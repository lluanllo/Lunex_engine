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
		inline Color LabelColor()    { return Color(0.70f, 0.70f, 0.70f, 1.0f); }
		inline Color ValueColor()    { return Color(0.95f, 0.95f, 0.95f, 1.0f); }
		inline Color HeaderColor()   { return Color(0.26f, 0.59f, 0.98f, 1.0f); }
		inline Color SectionBg()     { return Color(0.12f, 0.12f, 0.14f, 0.80f); }
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
