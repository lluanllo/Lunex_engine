/**
 * @file ComponentDrawer.cpp
 * @brief ECS Component UI Drawer Implementation
 */

#include "stpch.h"
#include "ComponentDrawer.h"

namespace Lunex::UI {

	void ComponentDrawer::End() {
		EndPropertyGrid();
		ImGui::Unindent(ComponentStyle::INDENT_SIZE);
		ImGui::TreePop();
	}
	
	void ComponentDrawer::DrawSectionHeader(const char* icon, const std::string& title) {
		ImGui::Spacing();
		ScopedColor textColor(ImGuiCol_Text, ComponentStyle::HeaderColor());
		if (icon && icon[0] != '\0') {
			ImGui::Text("%s  %s", icon, title.c_str());
		} else {
			ImGui::Text("%s", title.c_str());
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}
	
	void ComponentDrawer::BeginIndent() {
		ImGui::Indent(ComponentStyle::INDENT_SIZE);
	}
	
	void ComponentDrawer::EndIndent() {
		ImGui::Unindent(ComponentStyle::INDENT_SIZE);
	}
	
	bool ComponentDrawer::BeginInfoCard(const std::string& id, float height) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ToImVec4(ComponentStyle::BgDark()));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
		return ImGui::BeginChild(id.c_str(), ImVec2(-1, height), true, 
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	}
	
	void ComponentDrawer::EndInfoCard() {
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}
	
	void ComponentDrawer::DrawDropZone(const std::string& text, const Size& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(ComponentStyle::BgDark()));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::BgHover()));
		ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(ComponentStyle::AccentColor()));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
		
		ImGui::Button(text.c_str(), ToImVec2(size));
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
	}
	
	bool ComponentDrawer::AcceptDropPayload(const char* payloadType, void** outData) {
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType)) {
				*outData = payload->Data;
				ImGui::EndDragDropTarget();
				return true;
			}
			ImGui::EndDragDropTarget();
		}
		return false;
	}
	
	void ComponentDrawer::BeginPropertyGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	}
	
	void ComponentDrawer::EndPropertyGrid() {
		ImGui::PopStyleVar(2);
	}

} // namespace Lunex::UI
