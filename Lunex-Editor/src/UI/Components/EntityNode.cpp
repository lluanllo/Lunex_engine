/**
 * @file EntityNode.cpp
 * @brief Entity Node Component Implementation
 */

#include "stpch.h"
#include "EntityNode.h"

namespace Lunex::UI {

	EntityNodeResult EntityNode::Render(const std::string& label, uint64_t entityID, int depth,
										bool isSelected, bool hasChildren, bool isExpanded,
										Ref<Texture2D> icon) {
		EntityNodeResult result;
		
		ScopedID scopedID((int)entityID);
		
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 itemSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Render background
		RenderBackground(drawList, cursorScreenPos, itemSize, isSelected);
		
		// Indent based on depth
		if (depth > 0) {
			ImGui::Indent(depth * m_Style.indentPerLevel);
		}
		
		// Render icon
		if (icon) {
			RenderIcon(icon);
		}
		
		// Get tree node flags
		ImGuiTreeNodeFlags flags = GetTreeNodeFlags(hasChildren, isSelected);
		
		// Draw tree node
		result.expanded = ImGui::TreeNodeEx(label.c_str(), flags);
		
		// Hover effect
		if (ImGui::IsItemHovered()) {
			drawList->AddRectFilled(cursorScreenPos,
				ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y),
				ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.hoverColor)));
		}
		
		// Click handling
		result.clicked = ImGui::IsItemClicked();
		result.doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		result.rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		
		// Drag source
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			SetEntityPayload(entityID);
			ImGui::Text("?? %s", label.c_str());
			result.dragStarted = true;
			ImGui::EndDragDropSource();
		}
		
		// Drop target
		if (ImGui::BeginDragDropTarget()) {
			if (auto droppedID = AcceptEntityPayload()) {
				result.dropReceived = true;
				result.droppedEntityID = *droppedID;
			}
			ImGui::EndDragDropTarget();
		}
		
		// Unindent
		if (depth > 0) {
			ImGui::Unindent(depth * m_Style.indentPerLevel);
		}
		
		return result;
	}
	
	void EntityNode::RenderBackground(ImDrawList* drawList, ImVec2 pos, ImVec2 size, bool isSelected) {
		ImU32 bgColor = isSelected ? 
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.selectedColor)) :
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.backgroundColor));
		
		if (bgColor != 0) {
			drawList->AddRectFilled(pos, 
				ImVec2(pos.x + size.x, pos.y + size.y), 
				bgColor);
		}
	}
	
	void EntityNode::RenderIcon(Ref<Texture2D> icon) {
		ImVec2 iconPos = ImGui::GetCursorPos();
		ImGui::SetCursorPosY(iconPos.y + 2.0f);
		
		ImGui::Image(
			(ImTextureID)(intptr_t)icon->GetRendererID(),
			ImVec2(m_Style.iconSize, m_Style.iconSize),
			ImVec2(0, 1), ImVec2(1, 0)
		);
		
		ImGui::SameLine();
		ImGui::SetCursorPosY(iconPos.y);
	}
	
	ImGuiTreeNodeFlags EntityNode::GetTreeNodeFlags(bool hasChildren, bool isSelected) const {
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
								   ImGuiTreeNodeFlags_SpanAvailWidth |
								   ImGuiTreeNodeFlags_FramePadding;
		
		if (!hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		
		if (isSelected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		
		return flags;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	EntityNodeResult RenderEntityNode(const std::string& label, uint64_t entityID, int depth,
									  bool isSelected, bool hasChildren, bool isExpanded,
									  Ref<Texture2D> icon, const EntityNodeStyle& style) {
		EntityNode node;
		node.SetStyle(style);
		return node.Render(label, entityID, depth, isSelected, hasChildren, isExpanded, icon);
	}

} // namespace Lunex::UI
