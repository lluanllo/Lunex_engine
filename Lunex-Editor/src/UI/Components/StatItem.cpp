/**
 * @file StatItem.cpp
 * @brief Stat Item Component Implementation
 */

#include "StatItem.h"
#include <imgui.h>
#include <cstdio>

namespace Lunex::UI {

	void StatItem(const char* label, const char* value) {
		ImGui::PushStyleColor(ImGuiCol_Text, StatStyle::LabelColor().ToImVec4());
		ImGui::Text("%s: ", label);
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		ImGui::PushStyleColor(ImGuiCol_Text, StatStyle::ValueColor().ToImVec4());
		ImGui::Text("%s", value);
		ImGui::PopStyleColor();
	}

	void StatItem(const char* label, int value) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%d", value);
		StatItem(label, buffer);
	}

	void StatItem(const char* label, float value, const char* format) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), format, value);
		StatItem(label, buffer);
	}

	void StatItem(const char* label, uint32_t value) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%u", value);
		StatItem(label, buffer);
	}

	void StatHeader(const char* title) {
		ImGui::PushStyleColor(ImGuiCol_Text, StatStyle::HeaderColor().ToImVec4());
		ImGui::Text("%s", title);
		ImGui::PopStyleColor();
		ImGui::Separator();
	}

	bool BeginStatSection(const char* title) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, StatStyle::SectionBg().ToImVec4());
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		
		// Use ImGuiWindowFlags_AlwaysAutoResize for child windows
		bool open = ImGui::BeginChild(title, ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize);
		
		if (open) {
			StatHeader(title);
			ImGui::Spacing();
		}
		
		return open;
	}

	void EndStatSection() {
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

} // namespace Lunex::UI
