/**
 * @file UIWidgets.cpp
 * @brief Legacy widget implementations (Progress, Spinner, Shortcut)
 * 
 * Most widgets have been moved to Components/ directory.
 * This file contains simple utility functions that don't warrant
 * their own component class.
 */

#include "stpch.h"
#include "UIWidgets.h"

namespace Lunex::UI {

	// ============================================================================
	// PROGRESS INDICATOR
	// ============================================================================
	
	void ProgressBar(float progress, const Size& size, const char* overlay) {
		ImGui::ProgressBar(progress, ToImVec2(size), overlay);
	}
	
	void Spinner(const std::string& id, float radius, const Color& color) {
		ScopedID scopedID(id);
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float time = (float)ImGui::GetTime();
		int numSegments = 30;
		float start = fmodf(time * 3.0f, 2.0f * IM_PI);
		
		ImU32 col = ImGui::ColorConvertFloat4ToU32(ToImVec4(color));
		
		for (int i = 0; i < numSegments; i++) {
			float a = start + (float)i * 2.0f * IM_PI / (float)numSegments;
			float x = pos.x + radius + cosf(a) * radius;
			float y = pos.y + radius + sinf(a) * radius;
			
			float alpha = (float)i / (float)numSegments;
			ImU32 segCol = (col & 0x00FFFFFF) | ((int)(alpha * 255) << 24);
			
			drawList->AddCircleFilled(ImVec2(x, y), 2.0f, segCol);
		}
		
		ImGui::Dummy(ImVec2(radius * 2, radius * 2));
	}
	
	// ============================================================================
	// KEYBOARD SHORTCUTS DISPLAY
	// ============================================================================
	
	void KeyboardShortcut(const std::string& shortcut) {
		ScopedColor color(ImGuiCol_Text, Colors::TextMuted());
		ImGui::Text("%s", shortcut.c_str());
	}

} // namespace Lunex::UI
