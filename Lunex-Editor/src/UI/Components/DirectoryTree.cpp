/**
 * @file DirectoryTree.cpp
 * @brief Directory Tree Component Implementation
 */

#include "stpch.h"
#include "DirectoryTree.h"

namespace Lunex::UI {

	void DirectoryTree::Render(const std::filesystem::path& rootPath,
							   const std::string& rootLabel,
							   const std::filesystem::path& currentDirectory,
							   Ref<Texture2D> folderIcon,
							   const DirectoryTreeCallbacks& callbacks) {
		ScopedStyle vars(
			ImGuiStyleVar_IndentSpacing, m_Style.indentSpacing,
			ImGuiStyleVar_ItemSpacing, ImVec2(0, m_Style.itemSpacing)
		);
		
		AddSpacing(SpacingValues::SM);
		
		// Section label
		{
			ScopedColor labelColor(ImGuiCol_Text, m_Style.labelColor);
			Indent(8);
			ImGui::Text("FOLDERS");
			Unindent(8);
		}
		
		AddSpacing(SpacingValues::XS);
		
		// Tree colors
		ScopedColor colors({
			{ImGuiCol_Header, m_Style.headerColor},
			{ImGuiCol_HeaderHovered, m_Style.hoverColor},
			{ImGuiCol_HeaderActive, m_Style.selectedColor},
			{ImGuiCol_Text, m_Style.textColor}
		});
		
		// Root node
		ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow |
									   ImGuiTreeNodeFlags_SpanAvailWidth |
									   ImGuiTreeNodeFlags_DefaultOpen |
									   ImGuiTreeNodeFlags_FramePadding;
		
		if (currentDirectory == rootPath) {
			rootFlags |= ImGuiTreeNodeFlags_Selected;
		}
		
		ImVec2 rootCursorPos = ImGui::GetCursorScreenPos();
		
		// Add spacing for icon
		std::string displayRootLabel = "   " + rootLabel;
		bool rootOpened = ImGui::TreeNodeEx(displayRootLabel.c_str(), rootFlags);
		
		// Draw folder icon
		RenderFolderIcon(folderIcon, rootCursorPos);
		
		// Handle drag-drop on root
		HandleDragDropTarget(rootPath, callbacks);
		
		// Handle click on root
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			if (callbacks.onDirectorySelected) {
				callbacks.onDirectorySelected(rootPath);
			}
		}
		
		if (rootOpened) {
			// Render subdirectories
			for (auto& entry : std::filesystem::directory_iterator(rootPath)) {
				if (entry.is_directory()) {
					RenderDirectoryNode(entry.path(), currentDirectory, folderIcon, callbacks);
				}
			}
			ImGui::TreePop();
		}
	}
	
	void DirectoryTree::RenderDirectoryNode(const std::filesystem::path& path,
											const std::filesystem::path& currentDirectory,
											Ref<Texture2D> folderIcon,
											const DirectoryTreeCallbacks& callbacks) {
		std::string dirName = path.filename().string();
		
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
								   ImGuiTreeNodeFlags_SpanAvailWidth |
								   ImGuiTreeNodeFlags_FramePadding;
		
		if (path == currentDirectory) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		
		bool hasSubdirs = HasSubdirectories(path);
		if (!hasSubdirs) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		
		ScopedID nodeID(dirName);
		
		// Add spacing for icon
		std::string displayName = "   " + dirName;
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		
		bool opened = ImGui::TreeNodeEx(displayName.c_str(), flags);
		
		// Draw folder icon
		RenderFolderIcon(folderIcon, cursorPos);
		
		// Handle drag-drop
		HandleDragDropTarget(path, callbacks);
		
		// Handle click
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			if (callbacks.onDirectorySelected) {
				callbacks.onDirectorySelected(path);
			}
		}
		
		if (opened && hasSubdirs) {
			for (auto& entry : std::filesystem::directory_iterator(path)) {
				if (entry.is_directory()) {
					RenderDirectoryNode(entry.path(), currentDirectory, folderIcon, callbacks);
				}
			}
			ImGui::TreePop();
		}
	}
	
	void DirectoryTree::RenderFolderIcon(Ref<Texture2D> icon, ImVec2 cursorPos) {
		if (!icon) return;
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 iconPos = ImVec2(cursorPos.x + m_Style.arrowWidth, cursorPos.y + 2.0f);
		
		drawList->AddImage(
			(ImTextureID)(intptr_t)icon->GetRendererID(),
			iconPos,
			ImVec2(iconPos.x + m_Style.iconSize, iconPos.y + m_Style.iconSize),
			ImVec2(0, 0),
			ImVec2(1, 1)
		);
	}
	
	bool DirectoryTree::HasSubdirectories(const std::filesystem::path& path) {
		for (auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_directory()) {
				return true;
			}
		}
		return false;
	}
	
	void DirectoryTree::HandleDragDropTarget(const std::filesystem::path& targetPath,
											 const DirectoryTreeCallbacks& callbacks) {
		if (!ImGui::BeginDragDropTarget()) return;
		
		// Visual highlight
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 itemMin = ImGui::GetItemRectMin();
		ImVec2 itemMax = ImGui::GetItemRectMax();
		
		ImU32 highlightColor = IM_COL32(90, 150, 255, 80);
		drawList->AddRectFilled(itemMin, itemMax, highlightColor);
		
		ImU32 borderColor = IM_COL32(90, 150, 255, 200);
		drawList->AddRect(itemMin, itemMax, borderColor, 0.0f, 0, 2.0f);
		
		// Accept single item
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
			"CONTENT_BROWSER_ITEM",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
			
			if (payload->IsDelivery() && callbacks.onSingleItemDropped) {
				callbacks.onSingleItemDropped(targetPath, payload->Data, payload->DataSize);
			}
		}
		
		// Accept multiple items
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
			"CONTENT_BROWSER_ITEMS",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
			
			if (payload->IsDelivery() && callbacks.onFilesDropped) {
				std::string payloadData((const char*)payload->Data);
				callbacks.onFilesDropped(targetPath, payloadData);
			}
		}
		
		ImGui::EndDragDropTarget();
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	void RenderDirectoryTree(const std::filesystem::path& rootPath,
							 const std::string& rootLabel,
							 const std::filesystem::path& currentDirectory,
							 Ref<Texture2D> folderIcon,
							 const DirectoryTreeCallbacks& callbacks,
							 const DirectoryTreeStyle& style) {
		DirectoryTree tree;
		tree.SetStyle(style);
		tree.Render(rootPath, rootLabel, currentDirectory, folderIcon, callbacks);
	}

} // namespace Lunex::UI
