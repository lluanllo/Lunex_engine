#include "stpch.h"
#include "ContentBrowserPanel.h"
#include "Events/Event.h"
#include "Events/FileDropEvent.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "../Editor/UI/UIStyles.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <sstream>

namespace Lunex {

	const std::filesystem::path g_AssetPath = "assets";

	// ============================================================================
	// CONSTRUCTOR
	// ============================================================================
	ContentBrowserPanel::ContentBrowserPanel() {
		m_Navigation.Initialize(g_AssetPath);
		m_FileOperations.SetBaseDirectory(g_AssetPath);
		
		// Set up thumbnail invalidation callback
		m_FileOperations.SetOnThumbnailInvalidate([this](const std::filesystem::path& path) {
			m_ThumbnailManager.InvalidateThumbnail(path);
		});
	}

	void ContentBrowserPanel::SetRootDirectory(const std::filesystem::path& directory) {
		m_Navigation.Reset(directory);
		m_FileOperations.SetBaseDirectory(directory);
		m_ThumbnailManager.ClearAllCaches();
		m_Selection.Clear();
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================
	void ContentBrowserPanel::OnImGuiRender() {
		// Professional dark styling
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Content Browser", NULL, ImGuiWindowFlags_MenuBar);
		ImGui::PopStyleVar();

		RenderTopBar();

		// Horizontal layout with adjustable splitter
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		static float sidebarWidth = 200.0f;
		const float minSidebarWidth = 150.0f;
		const float maxSidebarWidth = 400.0f;

		ImVec2 availSize = ImGui::GetContentRegionAvail();

		// Sidebar
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
		ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, availSize.y), false);
		RenderSidebar();
		ImGui::EndChild();
		ImGui::PopStyleColor();

		// Sidebar shadow
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 sidebarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(sidebarMax.x, sidebarMax.y - availSize.y);

		for (int i = 0; i < 6; i++) {
			float alpha = (1.0f - (i / 6.0f)) * 0.25f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(shadowStart.x + i, shadowStart.y),
				ImVec2(shadowStart.x + i + 1, sidebarMax.y),
				shadowColor
			);
		}

		ImGui::SameLine();

		// Splitter
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.5f));
		ImGui::Button("##Splitter", ImVec2(4.0f, availSize.y));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemActive()) {
			sidebarWidth += ImGui::GetIO().MouseDelta.x;
			sidebarWidth = std::clamp(sidebarWidth, minSidebarWidth, maxSidebarWidth);
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		ImGui::SameLine();

		// File grid
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::BeginChild("FileGrid", ImVec2(0, availSize.y), false);
		RenderFileGrid();
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::PopStyleVar();

		ImGui::End();
		ImGui::PopStyleColor(4);

		// Render dialogs
		RenderCreateFolderDialog();
		RenderRenameDialog();
	}

	// ============================================================================
	// TOP BAR
	// ============================================================================
	void ContentBrowserPanel::RenderTopBar() {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.20f, 0.21f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		ImGui::BeginChild("TopBar", ImVec2(0, 40), true, ImGuiWindowFlags_NoScrollbar);

		// Back button
		bool canGoBack = m_Navigation.CanGoBack();
		if (!canGoBack) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);

		Ref<Texture2D> backIcon = m_ThumbnailManager.GetBackIcon();
		if (backIcon) {
			if (ImGui::ImageButton("##BackButton", (ImTextureID)(uintptr_t)backIcon->GetRendererID(),
				ImVec2(22, 22), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoBack) {
				m_Navigation.NavigateBack();
			}
		}
		else {
			if (ImGui::Button("<", ImVec2(30, 30)) && canGoBack) {
				m_Navigation.NavigateBack();
			}
		}

		if (!canGoBack) ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

		ImGui::SameLine();

		// Forward button
		bool canGoForward = m_Navigation.CanGoForward();
		if (!canGoForward) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);

		Ref<Texture2D> forwardIcon = m_ThumbnailManager.GetForwardIcon();
		if (forwardIcon) {
			if (ImGui::ImageButton("##ForwardButton", (ImTextureID)(uintptr_t)forwardIcon->GetRendererID(),
				ImVec2(22, 22), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoForward) {
				m_Navigation.NavigateForward();
			}
		}
		else {
			if (ImGui::Button(">", ImVec2(30, 30)) && canGoForward) {
				m_Navigation.NavigateForward();
			}
		}

		if (!canGoForward) ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(16, 0));
		ImGui::SameLine();

		// Path display
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.82f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::AlignTextToFramePadding();

		static char pathBuffer[512];
		std::string currentPath = m_Navigation.GetCurrentDirectory().string();
		strncpy_s(pathBuffer, sizeof(pathBuffer), currentPath.c_str(), _TRUNCATE);

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 220);
		ImGui::InputText("##PathDisplay", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);

		ImGui::PopStyleColor(2);

		// Search bar
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.22f, 0.23f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.87f, 1.0f));
		ImGui::SetNextItemWidth(200);
		ImGui::InputTextWithHint("##Search", "🔍 Search...", m_SearchBuffer, 256);
		ImGui::PopStyleColor(4);

		ImGui::EndChild();

		// Bottom shadow
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 topbarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(topbarMax.x - ImGui::GetContentRegionAvail().x, topbarMax.y);

		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.35f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(shadowStart.x, shadowStart.y + i),
				ImVec2(topbarMax.x, shadowStart.y + i + 1),
				shadowColor
			);
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(7);
	}

	// ============================================================================
	// SIDEBAR
	// ============================================================================
	void ContentBrowserPanel::RenderSidebar() {
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

		ImGui::Dummy(ImVec2(0, 8));

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
		ImGui::Indent(8);
		ImGui::Text("FOLDERS");
		ImGui::Unindent(8);
		ImGui::PopStyleColor();

		ImGui::Dummy(ImVec2(0, 4));

		// Tree styling
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.40f, 0.65f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));

		// Root "Assets" folder
		ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_FramePadding;

		if (m_Navigation.IsAtRoot()) {
			rootFlags |= ImGuiTreeNodeFlags_Selected;
		}

		ImVec2 rootCursorPos = ImGui::GetCursorScreenPos();
		bool rootOpened = ImGui::TreeNodeEx("   Assets", rootFlags);

		// Draw folder icon for Assets
		Ref<Texture2D> dirIcon = m_ThumbnailManager.GetDirectoryIcon();
		if (dirIcon) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			float iconSize = 16.0f;
			float arrowWidth = 20.0f;
			ImVec2 iconPos = ImVec2(rootCursorPos.x + arrowWidth, rootCursorPos.y + 2.0f);

			drawList->AddImage(
				(ImTextureID)(uintptr_t)dirIcon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				ImVec2(0, 0),
				ImVec2(1, 1)
			);
		}

		// Drag & drop target for Assets root
		SetupDragDropTarget(m_Navigation.GetBaseDirectory());

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			m_Navigation.NavigateTo(m_Navigation.GetBaseDirectory());
		}

		if (rootOpened) {
			RenderDirectoryTree(m_Navigation.GetBaseDirectory());
			ImGui::TreePop();
		}

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
	}

	void ContentBrowserPanel::RenderDirectoryTree(const std::filesystem::path& path) {
		for (auto& entry : std::filesystem::directory_iterator(path)) {
			if (!entry.is_directory()) continue;

			const auto& dirPath = entry.path();
			std::string dirName = dirPath.filename().string();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_FramePadding;

			if (dirPath == m_Navigation.GetCurrentDirectory()) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			bool hasSubdirs = false;
			for (auto& sub : std::filesystem::directory_iterator(dirPath)) {
				if (sub.is_directory()) {
					hasSubdirs = true;
					break;
				}
			}

			if (!hasSubdirs) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}

			ImGui::PushID(dirName.c_str());

			std::string displayName = "   " + dirName;
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();

			bool opened = ImGui::TreeNodeEx(displayName.c_str(), flags);

			// Draw folder icon
			Ref<Texture2D> dirIcon = m_ThumbnailManager.GetDirectoryIcon();
			if (dirIcon) {
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				float iconSize = 16.0f;
				float arrowWidth = 20.0f;
				ImVec2 iconPos = ImVec2(cursorPos.x + arrowWidth, cursorPos.y + 2.0f);

				drawList->AddImage(
					(ImTextureID)(uintptr_t)dirIcon->GetRendererID(),
					iconPos,
					ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
					ImVec2(0, 0),
					ImVec2(1, 1)
				);
			}

			// Drag & drop target
			SetupDragDropTarget(dirPath);

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				m_Navigation.NavigateTo(dirPath);
			}

			if (opened && hasSubdirs) {
				RenderDirectoryTree(dirPath);
				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	// ============================================================================
	// FILE GRID
	// ============================================================================
	void ContentBrowserPanel::RenderFileGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
		ImGui::BeginChild("FileGridContent", ImVec2(0, -28), false);

		m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		m_HoveredFolder.clear();
		m_Selection.ClearItemBounds();

		ImGui::Indent(16.0f);

		// Handle selection rectangle
		bool isCtrlDown = ImGui::GetIO().KeyCtrl;
		bool isShiftDown = ImGui::GetIO().KeyShift;

		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
			!isCtrlDown && !isShiftDown) {
			ImVec2 mousePos = ImGui::GetMousePos();
			m_Selection.BeginRectangleSelection(mousePos.x, mousePos.y);
			m_Selection.Clear();
		}

		if (m_Selection.IsRectangleSelecting() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ImVec2 mousePos = ImGui::GetMousePos();
			m_Selection.UpdateRectangleSelection(mousePos.x, mousePos.y);
		}

		if (m_Selection.IsRectangleSelecting() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			m_Selection.EndRectangleSelection();
		}

		// Context menu
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("FileGridContextMenu");
		}

		RenderContextMenu();

		// Calculate layout
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = m_CardRenderer.CalculateColumnCount(panelWidth);
		float cellSize = m_CardRenderer.GetCellSize();
		float padding = 12.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(padding, padding + 8));

		// Search filter
		std::string searchQuery = m_SearchBuffer;
		std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);

		// Grid colors
	 ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.65f, 0.8f));

		// Manual layout for variable-width items
		float startX = ImGui::GetCursorScreenPos().x;
		float startY = ImGui::GetCursorScreenPos().y;
		float currentX = 0.0f;
		float currentY = 0.0f;
		float rowHeight = 0.0f;
		int currentColumn = 0;

		for (auto& directoryEntry : std::filesystem::directory_iterator(m_Navigation.GetCurrentDirectory())) {
			const auto& path = directoryEntry.path();
			std::string filenameString = path.filename().string();

			// Apply search filter
			if (!searchQuery.empty()) {
				std::string filenameLower = filenameString;
				std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
				if (filenameLower.find(searchQuery) == std::string::npos) {
					continue;
				}
			}

			// Determine item width
			bool isHDR = m_ThumbnailManager.IsHDRFile(path);
			bool isDirectory = directoryEntry.is_directory();

			float itemWidth = isDirectory ? cellSize : 
				(isHDR ? m_CardRenderer.GetFileCardWidth(true) + padding * 2 : cellSize);
			int columnsNeeded = isHDR ? 2 : 1;

			// Check if item fits on current row
			if (currentColumn > 0 && (currentColumn + columnsNeeded > columnCount || 
				currentX + itemWidth > panelWidth - 32)) {
				currentX = 0.0f;
				currentY += rowHeight + padding + 8;
				rowHeight = 0.0f;
				currentColumn = 0;
			}

			// Set cursor position
			ImGui::SetCursorScreenPos(ImVec2(startX + currentX, startY + currentY));

			ImGui::PushID(filenameString.c_str());

			bool isSelected = m_Selection.IsSelected(path);

			if (isDirectory) {
				// Render folder
				Ref<Texture2D> icon = m_ThumbnailManager.GetDirectoryIcon();
				auto result = m_CardRenderer.RenderFolderCard(path, icon, isSelected, false);

				m_Selection.RegisterItemBounds(path, result.Bounds.Min.x, result.Bounds.Min.y, 
					result.Bounds.Max.x, result.Bounds.Max.y);

				if (result.Clicked) {
					HandleItemClick(path, isCtrlDown, isShiftDown);
				}

				if (result.DoubleClicked) {
					HandleItemDoubleClick(path, true);
				}

				if (result.RightClicked) {
					HandleItemRightClick(path);
				}

				if (ImGui::IsItemHovered()) {
					m_HoveredFolder = path;
				}

				// Drag & drop
				SetupDragDropSource(path, true);
				SetupDragDropTarget(path);

				rowHeight = std::max(rowHeight, m_CardRenderer.GetFolderCardHeight());
			}
			else {
				// Get thumbnail
				Ref<Texture2D> thumbnail;
				std::string ext = path.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				if (ext == ".lumat") {
					auto material = MaterialRegistry::Get().LoadMaterial(path);
					thumbnail = m_ThumbnailManager.GetMaterialThumbnail(path, material);
				}
				else if (ext == ".luprefab") {
					thumbnail = m_ThumbnailManager.GetPrefabThumbnail(path, m_Navigation.GetBaseDirectory());
				}
				else {
					thumbnail = m_ThumbnailManager.GetThumbnailForFile(path);
				}

				std::string typeLabel = m_ThumbnailManager.GetAssetTypeLabel(path);
				ImU32 borderColor = m_ThumbnailManager.GetAssetTypeBorderColor(path);

				auto result = m_CardRenderer.RenderFileCard(path, thumbnail, typeLabel, 
					borderColor, isSelected, false, isHDR);

				m_Selection.RegisterItemBounds(path, result.Bounds.Min.x, result.Bounds.Min.y,
					result.Bounds.Max.x, result.Bounds.Max.y);

				if (result.Clicked) {
					HandleItemClick(path, isCtrlDown, isShiftDown);
				}

				if (result.DoubleClicked) {
					HandleItemDoubleClick(path, false);
				}

				if (result.RightClicked) {
					HandleItemRightClick(path);
				}

				// Drag & drop source
				SetupDragDropSource(path, false);

				rowHeight = std::max(rowHeight, m_CardRenderer.GetFileCardHeight(isHDR));
			}

			ImGui::PopID();

			// Update layout
			currentX += itemWidth + padding;
			currentColumn += columnsNeeded;
		}

		// Check rectangle selection
		if (m_Selection.IsRectangleSelecting()) {
			m_Selection.CheckRectangleIntersection();
		}

		// Deselect when clicking empty space
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!ImGui::IsAnyItemHovered() && !isCtrlDown && !isShiftDown && 
			!m_Selection.IsRectangleSelecting()) {
			m_Selection.Clear();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		ImGui::Unindent(16.0f);

		// Draw selection rectangle
		if (m_Selection.IsRectangleSelecting()) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			float startX, startY, endX, endY;
			m_Selection.GetSelectionRectStart(startX, startY);
			m_Selection.GetSelectionRectEnd(endX, endY);

			ImVec2 rectMin(std::min(startX, endX), std::min(startY, endY));
			ImVec2 rectMax(std::max(startX, endX), std::max(startY, endY));

			drawList->AddRectFilled(rectMin, rectMax, IM_COL32(90, 150, 255, 50));
			drawList->AddRect(rectMin, rectMax, IM_COL32(90, 150, 255, 200), 0.0f, 0, 2.0f);
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();

		// Bottom bar with size slider
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 bottomBarStart = ImGui::GetCursorScreenPos();

		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.3f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(bottomBarStart.x, bottomBarStart.y - i),
				ImVec2(bottomBarStart.x + ImGui::GetContentRegionAvail().x, bottomBarStart.y - i + 1),
				shadowColor
			);
		}

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.50f, 0.50f, 0.50f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.70f, 0.70f, 0.70f, 1.0f));

		ImGui::BeginChild("BottomBar", ImVec2(0, 0), false);
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170);
		ImGui::SetCursorPosY(6);
		ImGui::SetNextItemWidth(160);

		static float thumbnailSize = 96.0f;
		if (ImGui::SliderFloat("##Size", &thumbnailSize, 64, 160)) {
			m_CardRenderer.SetThumbnailSize(thumbnailSize);
		}
		ImGui::EndChild();

		ImGui::PopStyleColor(4);
	}

	// ============================================================================
	// CONTEXT MENU
	// ============================================================================
	void ContentBrowserPanel::RenderContextMenu() {
		if (ImGui::BeginPopup("FileGridContextMenu")) {
			ImGui::SeparatorText("Create");

			if (ImGui::MenuItem("New Folder")) {
				m_ShowCreateFolderDialog = true;
				strcpy_s(m_NewItemName, "NewFolder");
			}

			if (ImGui::MenuItem("New Scene")) {
				m_FileOperations.CreateNewScene(m_Navigation.GetCurrentDirectory());
			}

			if (ImGui::MenuItem("New Script")) {
				m_FileOperations.CreateNewScript(m_Navigation.GetCurrentDirectory());
			}

			if (ImGui::MenuItem("New Material")) {
				m_FileOperations.CreateNewMaterial(m_Navigation.GetCurrentDirectory());
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Open in Explorer")) {
				std::string command = "explorer " + m_Navigation.GetCurrentDirectory().string();
				system(command.c_str());
			}

			if (ImGui::MenuItem("Refresh")) {
				m_ThumbnailManager.ClearAllCaches();
			}

			ImGui::EndPopup();
		}
	}

	// ============================================================================
	// DIALOGS
	// ============================================================================
	void ContentBrowserPanel::RenderCreateFolderDialog() {
		if (m_ShowCreateFolderDialog) {
			ImGui::OpenPopup("Create New Folder");
			m_ShowCreateFolderDialog = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Create New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter folder name:");
			ImGui::Spacing();

			ImGui::SetNextItemWidth(300);
			if (ImGui::InputText("##FolderName", m_NewItemName, sizeof(m_NewItemName), 
				ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_FileOperations.CreateNewFolder(m_Navigation.GetCurrentDirectory(), m_NewItemName);
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}

			ImGui::Spacing();

			if (ImGui::Button("Create", ImVec2(145, 0))) {
				m_FileOperations.CreateNewFolder(m_Navigation.GetCurrentDirectory(), m_NewItemName);
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(145, 0))) {
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ContentBrowserPanel::RenderRenameDialog() {
		if (m_ShowRenameDialog) {
			ImGui::OpenPopup("Rename Item");
			m_ShowRenameDialog = false;
		}

		if (ImGui::BeginPopupModal("Rename Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter new name:");
			ImGui::Spacing();

			ImGui::SetNextItemWidth(300);
			if (ImGui::InputText("##ItemName", m_NewItemName, sizeof(m_NewItemName), 
				ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_FileOperations.RenameItem(m_ItemToRename, m_NewItemName);
				ImGui::CloseCurrentPopup();
			}

			ImGui::Spacing();

			if (ImGui::Button("Rename", ImVec2(145, 0))) {
				m_FileOperations.RenameItem(m_ItemToRename, m_NewItemName);
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(145, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// ============================================================================
	// EVENT HANDLERS
	// ============================================================================
	void ContentBrowserPanel::HandleItemClick(const std::filesystem::path& path, 
		bool ctrlDown, bool shiftDown) {
		if (ctrlDown) {
			m_Selection.ToggleSelection(path);
		}
		else if (shiftDown && !m_Selection.GetLastSelected().empty()) {
			m_Selection.SelectRange(m_Selection.GetLastSelected(), path, 
				m_Navigation.GetCurrentDirectory());
		}
		else {
			if (!m_Selection.IsSelected(path)) {
				m_Selection.Clear();
				m_Selection.AddToSelection(path);
			}
		}
	}

	void ContentBrowserPanel::HandleItemDoubleClick(const std::filesystem::path& path, bool isDirectory) {
		if (isDirectory) {
			m_Navigation.NavigateTo(path);
		}
		else {
			std::string ext = path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			if (ext == ".lumat") {
				if (m_OnMaterialOpenCallback) {
					m_OnMaterialOpenCallback(path);
				}
				else {
					LNX_LOG_WARN("Material editor not connected");
				}
			}
			else if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") {
				std::string command = "cmd /c start \"\" \"" + path.string() + "\"";
				system(command.c_str());
			}
			else if (ext == ".lunex") {
				LNX_LOG_INFO("Double-clicked scene: {0}", path.filename().string());
			}
			else {
				std::string command = "cmd /c start \"\" \"" + path.string() + "\"";
				system(command.c_str());
			}
		}
	}

	void ContentBrowserPanel::HandleItemRightClick(const std::filesystem::path& path) {
		if (!m_Selection.IsSelected(path)) {
			m_Selection.Clear();
			m_Selection.AddToSelection(path);
		}

		std::string filename = path.filename().string();
		ImGui::OpenPopup(("ItemContextMenu##" + filename).c_str());

		if (ImGui::BeginPopup(("ItemContextMenu##" + filename).c_str())) {
			if (m_Selection.GetSelectionCount() > 1) {
				ImGui::Text("%zu items selected", m_Selection.GetSelectionCount());
			}
			else {
				ImGui::Text("%s", filename.c_str());
			}
			ImGui::Separator();

			std::string ext = path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			if (ext == ".lumesh" && m_Selection.GetSelectionCount() == 1) {
				if (ImGui::MenuItem("Create Prefab")) {
					m_FileOperations.CreatePrefabFromMesh(path, m_Navigation.GetBaseDirectory());
				}
				ImGui::Separator();
			}

			if (m_Selection.GetSelectionCount() == 1) {
				if (ImGui::MenuItem("Rename")) {
					m_ItemToRename = path;
					strncpy_s(m_NewItemName, sizeof(m_NewItemName), filename.c_str(), _TRUNCATE);
					m_ShowRenameDialog = true;
				}
			}

			if (ImGui::MenuItem("Delete")) {
				for (const auto& selectedPath : m_Selection.GetSelectedItems()) {
					m_FileOperations.DeleteItem(std::filesystem::path(selectedPath));
				}
				m_Selection.Clear();
			}

			if (m_Selection.GetSelectionCount() == 1) {
				ImGui::Separator();

				if (ImGui::MenuItem("Show in Explorer")) {
					std::string command = "explorer /select," + path.string();
					system(command.c_str());
				}

				if (std::filesystem::is_directory(path)) {
					if (ImGui::MenuItem("Open in File Explorer")) {
						std::string command = "explorer " + path.string();
						system(command.c_str());
					}
				}
			}

			ImGui::EndPopup();
		}
	}

	// ============================================================================
	// DRAG & DROP
	// ============================================================================
	void ContentBrowserPanel::SetupDragDropSource(const std::filesystem::path& path, bool isDirectory) {
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			if (m_Selection.IsSelected(path) && m_Selection.GetSelectionCount() > 1) {
				std::string payloadData;
				for (const auto& selectedPath : m_Selection.GetSelectedItems()) {
					std::filesystem::path sp(selectedPath);
					auto relPath = std::filesystem::relative(sp, g_AssetPath);
					payloadData += relPath.string() + "\n";
				}
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEMS",
					payloadData.c_str(), payloadData.size() + 1);
				ImGui::Text("%zu items", m_Selection.GetSelectionCount());
			}
			else {
				ContentBrowserPayload payload = {};

				std::filesystem::path fullPath = path;
				auto relativePath = std::filesystem::relative(fullPath, g_AssetPath);

				strncpy_s(payload.FilePath, fullPath.string().c_str(), _TRUNCATE);
				strncpy_s(payload.RelativePath, relativePath.string().c_str(), _TRUNCATE);

				std::string ext = path.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				strncpy_s(payload.Extension, ext.c_str(), _TRUNCATE);

				payload.IsDirectory = isDirectory;
				payload.ItemCount = 1;

				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &payload, sizeof(ContentBrowserPayload));
				ImGui::Text("%s", path.filename().string().c_str());
			}

			ImGui::EndDragDropSource();
		}
	}

	void ContentBrowserPanel::SetupDragDropTarget(const std::filesystem::path& targetFolder) {
		if (ImGui::BeginDragDropTarget()) {
			// Visual highlight
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 itemMin = ImGui::GetItemRectMin();
			ImVec2 itemMax = ImGui::GetItemRectMax();

			drawList->AddRectFilled(itemMin, itemMax, IM_COL32(90, 150, 255, 80));
			drawList->AddRect(itemMin, itemMax, IM_COL32(90, 150, 255, 200), 0.0f, 0, 2.0f);

			// Accept single item
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM",
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {

				if (payload->IsDelivery()) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					m_FileOperations.MoveItem(std::filesystem::path(data->FilePath), targetFolder);
				}
			}

			// Accept multiple items
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEMS",
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {

				if (payload->IsDelivery()) {
					std::string payloadData = (const char*)payload->Data;
					std::stringstream ss(payloadData);
					std::string line;

					while (std::getline(ss, line)) {
						if (line.empty()) continue;
						std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
						m_FileOperations.MoveItem(sourcePath, targetFolder);
					}

					m_Selection.Clear();
				}
			}

			ImGui::EndDragDropTarget();
		}
	}

	// ============================================================================
	// EVENT HANDLING
	// ============================================================================
	void ContentBrowserPanel::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<FileDropEvent>([this](FileDropEvent& event) {
			if (m_IsHovered) {
				HandleFileDrop(event.GetPaths());
				return true;
			}
			return false;
		});
	}

	void ContentBrowserPanel::HandleFileDrop(const std::vector<std::string>& files) {
		if (!m_HoveredFolder.empty() && std::filesystem::exists(m_HoveredFolder)) {
			m_FileOperations.ImportFilesToFolder(files, m_HoveredFolder);
		}
		else {
			m_FileOperations.ImportFiles(files, m_Navigation.GetCurrentDirectory());
		}
	}

	// ============================================================================
	// PUBLIC API
	// ============================================================================
	void ContentBrowserPanel::InvalidateMaterialThumbnail(const std::filesystem::path& materialPath) {
		m_ThumbnailManager.InvalidateThumbnail(materialPath);
	}

	void ContentBrowserPanel::InvalidateThumbnailDiskCache(const std::filesystem::path& materialPath) {
		m_ThumbnailManager.InvalidateMaterialDiskCache(materialPath);
	}

	void ContentBrowserPanel::RefreshAllThumbnails() {
		m_ThumbnailManager.ClearAllCaches();
	}

	void ContentBrowserPanel::SelectAll() {
		m_Selection.SelectAll(m_Navigation.GetCurrentDirectory(), m_SearchBuffer);
	}

	void ContentBrowserPanel::DeleteSelectedItems() {
		for (const auto& selectedPath : m_Selection.GetSelectedItems()) {
			m_FileOperations.DeleteItem(std::filesystem::path(selectedPath));
		}
		m_Selection.Clear();
	}

	void ContentBrowserPanel::DuplicateSelectedItem() {
		if (m_Selection.GetSelectionCount() == 1) {
			m_FileOperations.DuplicateItem(
				std::filesystem::path(*m_Selection.GetSelectedItems().begin()));
		}
	}

	void ContentBrowserPanel::RenameSelectedItem() {
		if (m_Selection.GetSelectionCount() == 1) {
			m_ItemToRename = std::filesystem::path(*m_Selection.GetSelectedItems().begin());
			strncpy_s(m_NewItemName, sizeof(m_NewItemName),
				m_ItemToRename.filename().string().c_str(), _TRUNCATE);
			m_ShowRenameDialog = true;
		}
	}

	void ContentBrowserPanel::PasteItems() {
		m_Selection.Paste(m_Navigation.GetCurrentDirectory());
	}

} // namespace Lunex
