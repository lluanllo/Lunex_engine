/**
 * @file UIState.h
 * @brief UI State and Utility Functions
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// DISABLED STATE
	// ============================================================================
	
	void BeginDisabled(bool disabled = true);
	void EndDisabled();
	
	// ============================================================================
	// ITEM STATE QUERIES
	// ============================================================================
	
	bool IsItemHovered();
	void SetTooltip(const char* fmt, ...);

} // namespace Lunex::UI
