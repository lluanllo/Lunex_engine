/**
 * @file UIState.cpp
 * @brief UI State and Utility Functions Implementation
 */

#include "stpch.h"
#include "UIState.h"

namespace Lunex::UI {

	void BeginDisabled(bool disabled) {
		ImGui::BeginDisabled(disabled);
	}
	
	void EndDisabled() {
		ImGui::EndDisabled();
	}
	
	bool IsItemHovered() {
		return ImGui::IsItemHovered();
	}
	
	void SetTooltip(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ImGui::SetTooltipV(fmt, args);
		va_end(args);
	}

} // namespace Lunex::UI
