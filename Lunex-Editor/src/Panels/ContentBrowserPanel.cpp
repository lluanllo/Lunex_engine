/**
 * @file ContentBrowserPanel.cpp
 * @brief Content Browser Panel Implementation using Lunex UI Framework
 */

#include "stpch.h"
#include "ContentBrowserPanel.h"
#include "Events/Event.h"
#include "Events/FileDropEvent.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Asset/Prefab.h"
#include "Assets/Core/AssetRegistry.h"
#include "Assets/Core/AssetDatabase.h"
#include "Project/ProjectManager.h"
#include <sstream>

namespace Lunex {
	const std::filesystem::path g_AssetPath = "assets";

	// ============================================================================
	// PANEL STYLE CONSTANTS
	// ============================================================================
	namespace PanelColors {
		static constexpr UI::Color WindowBg(0.082f, 0.102f, 0.129f, 1.0f);   // #151A21 blue-tinted
		static constexpr UI::Color ChildBg(0.082f, 0.102f, 0.129f, 1.0f);
		static constexpr UI::Color Border(0.06f, 0.08f, 0.10f, 1.0f);        // Blue-tinted dark border
		static constexpr UI::Color SidebarBg(0.071f, 0.090f, 0.114f, 1.0f);  // Slightly darker blue-tinted
		static constexpr UI::Color FileGridBg(0.102f, 0.125f, 0.157f, 1.0f); // #1A2028 blue-tinted
		static constexpr UI::Color BottomBarBg(0.06f, 0.08f, 0.10f, 1.0f);   // Blue-tinted very dark
	}

	// ============================================================================
	// CONSTRUCTOR
	// ============================================================================
	
	ContentBrowserPanel::ContentBrowserPanel()
		: m_BaseDirectory(g_AssetPath), m_CurrentDirectory(g_AssetPath)
	{
		// Load icons
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FolderIcon.png");
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
		m_BackIcon = Texture2D::Create("Resources/Icons/ContentBrowser/BackIcon.png");
		m_ForwardIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ForwardIcon.png");

		m_SceneIcon = Texture2D::Create("Resources/Icons/ContentBrowser/SceneIcon.png");
		m_TextureIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ImageIcon.png");
		m_ShaderIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ShaderIcon.png");
		m_AudioIcon = Texture2D::Create("Resources/Icons/ContentBrowser/AudioIcon.png");
		m_ScriptIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ScriptIcon.png");
		m_MaterialIcon = Texture2D::Create("Resources/Icons/ContentBrowser/MaterialIcon.png");
		m_MeshIcon = Texture2D::Create("Resources/Icons/ContentBrowser/MeshIcon.png");
		m_PrefabIcon = Texture2D::Create("Resources/Icons/ContentBrowser/PrefabIcon.png");

		// Initialize history
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;

		// Initialize material preview renderer
		m_MaterialPreviewRenderer = CreateScope<MaterialPreviewRenderer>();
		m_MaterialPreviewRenderer->SetResolution(160, 160);
		m_MaterialPreviewRenderer->SetAutoRotate(false);
		m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.432f, 0.757f, 1.0f, 1.0f));
		m_MaterialPreviewRenderer->SetCameraPosition(glm::vec3(0.0f, -0.3f, 2.5f));
		
		// Configure FileGrid style
		auto& gridStyle = m_FileGrid.GetStyle();
		gridStyle.thumbnailSize = m_ThumbnailSize;
	}

	void ContentBrowserPanel::SetRootDirectory(const std::filesystem::path& directory) {
		m_BaseDirectory = directory;
		m_CurrentDirectory = directory;

		m_DirectoryHistory.clear();
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;

		m_TextureCache.clear();
		m_MaterialThumbnailCache.clear();
		m_MeshThumbnailCache.clear();

		ClearSelection();
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void ContentBrowserPanel::OnImGuiRender() {
		using namespace UI;
		
		// Panel styling
		ScopedColor panelColors({
			{ImGuiCol_WindowBg, PanelColors::WindowBg},
			{ImGuiCol_ChildBg, PanelColors::ChildBg},
			{ImGuiCol_Border, PanelColors::Border},
			{ImGuiCol_BorderShadow, Color(0.0f, 0.0f, 0.0f, 0.6f)}
		});
		
		ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		
		if (!BeginPanel("Content Browser", nullptr, ImGuiWindowFlags_MenuBar)) {
			EndPanel();
			return;
		}
		
		// Track focus for auto-deselect when clicking another panel
		bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (m_WasFocused && !isFocused && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
			ClearSelection();
		}
		m_WasFocused = isFocused;
		
		// Top navigation bar
		RenderTopBar();
		
		// Main content area with sidebar and file grid
		{
			ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			
			ImVec2 availSize = ImGui::GetContentRegionAvail();
			
			// Sidebar
			{
				ScopedColor sidebarBg(ImGuiCol_ChildBg, PanelColors::SidebarBg);
				
				if (BeginChild("Sidebar", Size(m_SidebarWidth, availSize.y), false)) {
					RenderSidebar();
				}
				EndChild();
			}
			
			// Draw sidebar shadow
			{
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
			}
			
			SameLine();
			
			// Splitter
			VerticalSplitter("##ContentSplitter", m_SidebarWidth, MIN_SIDEBAR_WIDTH, MAX_SIDEBAR_WIDTH, availSize.y);
			
			SameLine();
			
			// File grid
			{
				ScopedColor gridBg(ImGuiCol_ChildBg, PanelColors::FileGridBg);
				
				if (BeginChild("FileGrid", Size(0, availSize.y), false)) {
					RenderFileGrid();
				}
				EndChild();
			}
		}
		
		EndPanel();
	}

	// ============================================================================
	// TOP BAR RENDERING
	// ============================================================================

	void ContentBrowserPanel::RenderTopBar() {
		using namespace UI;
		
		bool canGoBack = m_HistoryIndex > 0;
		bool canGoForward = m_HistoryIndex < (int)m_DirectoryHistory.size() - 1;
		
		NavigationBarCallbacks callbacks;
		callbacks.onBackClicked = [this]() { NavigateBack(); };
		callbacks.onForwardClicked = [this]() { NavigateForward(); };
		
		m_NavigationBar.Render(
			m_CurrentDirectory,
			canGoBack,
			canGoForward,
			m_BackIcon,
			m_ForwardIcon,
			m_SearchBuffer,
			sizeof(m_SearchBuffer),
			callbacks
		);
	}

	// ============================================================================
	// SIDEBAR RENDERING
	// ============================================================================

	void ContentBrowserPanel::RenderSidebar() {
		using namespace UI;
		
		DirectoryTreeCallbacks callbacks;
		callbacks.onDirectorySelected = [this](const std::filesystem::path& path) {
			OnDirectorySelected(path);
		};
		callbacks.onSingleItemDropped = [this](const std::filesystem::path& target, const void* data, size_t size) {
			OnDirectorySingleItemDropped(target, data, size);
		};
		callbacks.onFilesDropped = [this](const std::filesystem::path& target, const std::string& data) {
			OnDirectoryMultiItemDropped(target, data);
		};
		
		m_DirectoryTree.Render(
			m_BaseDirectory,
			"Assets",
			m_CurrentDirectory,
			m_DirectoryIcon,
			callbacks
		);
	}

	// ============================================================================
	// FILE GRID RENDERING
	// ============================================================================

	void ContentBrowserPanel::RenderFileGrid() {
		using namespace UI;
		
		// Update grid style with current thumbnail size
		m_FileGrid.GetStyle().thumbnailSize = m_ThumbnailSize;
		
		ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
		
		if (BeginChild("FileGridContent", Size(0, -28), false)) {
			m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
			
			Indent(16.0f);
			
			// Build items
			auto items = BuildFileGridItems();
			
			// Setup callbacks
			FileGridCallbacks callbacks;
			callbacks.onItemClicked = [this](const std::filesystem::path& path) {
				OnFileGridItemClicked(path);
			};
			callbacks.onItemDoubleClicked = [this](const std::filesystem::path& path) {
				OnFileGridItemDoubleClicked(path);
			};
			callbacks.onItemRightClicked = [this](const std::filesystem::path& path) {
				OnFileGridItemRightClicked(path);
			};
			callbacks.onItemDropped = [this](const std::filesystem::path& target, const void* data, size_t size) {
				OnFileGridItemDropped(target, data, size);
			};
			callbacks.onMultiItemDropped = [this](const std::filesystem::path& target, const std::string& data) {
				OnFileGridMultiItemDropped(target, data);
			};
			callbacks.onEmptySpaceClicked = [this]() {
				ClearSelection();
			};
			callbacks.onEmptySpaceRightClicked = [this]() {
				m_OpenEmptySpaceContextMenu = true;
			};
			
			// Render grid
			auto result = m_FileGrid.Render(items, m_SelectedItems, callbacks);
			m_HoveredFolder = result.hoveredFolder;
			
			// Context menus
			if (m_OpenEmptySpaceContextMenu) {
				OpenPopup("FileGridContextMenu");
				m_OpenEmptySpaceContextMenu = false;
			}
			
			RenderContextMenu();
			
			// Item context menu
			if (m_OpenItemContextMenu) {
				OpenPopup("ItemContextMenu");
				m_OpenItemContextMenu = false;
			}
			
			if (!m_ContextMenuTargetItem.empty()) {
				ScopedColor popupColors({
					{ImGuiCol_PopupBg, Color(0.10f, 0.13f, 0.16f, 0.98f)},
					{ImGuiCol_Border, PanelColors::Border},
					{ImGuiCol_Header, Color(0.13f, 0.16f, 0.20f, 1.0f)},
					{ImGuiCol_HeaderHovered, Color(0.055f, 0.647f, 0.769f, 0.35f)},
					{ImGuiCol_HeaderActive, Color(0.055f, 0.647f, 0.769f, 0.50f)},
					{ImGuiCol_Text, Colors::TextPrimary()},
					{ImGuiCol_Separator, PanelColors::Border}
				});
				ScopedStyle popupPadding(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
				ScopedStyle popupRounding(ImGuiStyleVar_PopupRounding, 6.0f);
				ScopedStyle popupItemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
				ScopedStyle popupFramePadding(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
				
				if (BeginPopup("ItemContextMenu")) {
					RenderItemContextMenu(m_ContextMenuTargetItem);
					EndPopup();
				}
			}
			
			Unindent(16.0f);
		}
		EndChild();
		
		// Bottom bar with size slider
		RenderBottomBar();
	}

	void ContentBrowserPanel::RenderBottomBar() {
		using namespace UI;
		
		// Shadow above bar
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 barStart = ImGui::GetCursorScreenPos();
		
		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.3f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(barStart.x, barStart.y - i),
				ImVec2(barStart.x + ImGui::GetContentRegionAvail().x, barStart.y - i + 1),
				shadowColor
			);
		}
		
		ScopedColor barColors({
			{ImGuiCol_ChildBg, PanelColors::BottomBarBg},
			{ImGuiCol_FrameBg, Color(0.09f, 0.11f, 0.14f, 1.0f)},
			{ImGuiCol_SliderGrab, Color(0.20f, 0.24f, 0.30f, 1.0f)},
			{ImGuiCol_SliderGrabActive, Colors::Primary()}
		});
		
		if (BeginChild("BottomBar", Size(0, 0), false)) {
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170);
			ImGui::SetCursorPosY(6);
			ImGui::SetNextItemWidth(160);
			ImGui::SliderFloat("##Size", &m_ThumbnailSize, 64, 160);
		}
		EndChild();
	}

	// ============================================================================
	// BUILD FILE GRID ITEMS
	// ============================================================================

	std::vector<UI::FileGridItem> ContentBrowserPanel::BuildFileGridItems() {
		std::vector<UI::FileGridItem> items;
		
		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			const auto& path = entry.path();
			std::string filename = path.filename().string();
			
			// Apply search filter
			if (!MatchesSearchFilter(filename)) {
				continue;
			}
			
			UI::FileGridItem item;
			item.path = path;
			item.name = GetDisplayName(path);
			item.isDirectory = entry.is_directory();
			item.typeLabel = GetAssetTypeLabel(path);
			item.thumbnail = item.isDirectory ? m_DirectoryIcon : GetThumbnailForFile(path);
			
			// Check if HDR file
			std::string ext = path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			item.isHDR = (ext == ".hdr" || ext == ".hdri");
			
			items.push_back(item);
		}
		
		return items;
	}

	std::string ContentBrowserPanel::GetDisplayName(const std::filesystem::path& path) {
		std::string name = path.filename().string();
		
		// Hide extensions for engine assets
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lumat" || ext == ".lumesh" || ext == ".luprefab") {
			name = path.stem().string();
		}
		
		return name;
	}

	bool ContentBrowserPanel::MatchesSearchFilter(const std::string& filename) {
		std::string searchQuery = m_SearchBuffer;
		if (searchQuery.empty()) return true;
		
		std::string filenameLower = filename;
		std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
		std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
		
		return filenameLower.find(searchQuery) != std::string::npos;
	}

	// ============================================================================
	// CONTEXT MENUS
	// ============================================================================

	void ContentBrowserPanel::RenderContextMenu() {
		using namespace UI;
		
		ScopedColor popupColors({
			{ImGuiCol_PopupBg, Color(0.10f, 0.13f, 0.16f, 0.98f)},
			{ImGuiCol_Border, PanelColors::Border},
			{ImGuiCol_Header, Color(0.13f, 0.16f, 0.20f, 1.0f)},
			{ImGuiCol_HeaderHovered, Color(0.055f, 0.647f, 0.769f, 0.35f)},
			{ImGuiCol_HeaderActive, Color(0.055f, 0.647f, 0.769f, 0.50f)},
			{ImGuiCol_Text, Colors::TextPrimary()},
			{ImGuiCol_Separator, PanelColors::Border}
		});
		ScopedStyle popupPadding(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ScopedStyle popupRounding(ImGuiStyleVar_PopupRounding, 6.0f);
		ScopedStyle popupItemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
		ScopedStyle popupFramePadding(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
		
		if (BeginPopup("FileGridContextMenu")) {
			SeparatorText("Create");
			
			if (MenuItem("New Folder")) {
				CreateNewFolder();
			}
			if (MenuItem("New Scene")) {
				CreateNewScene();
			}
			if (MenuItem("New Script")) {
				CreateNewScript();
			}
			if (MenuItem("New Material")) {
				CreateNewMaterial();
			}
			
			Separator();
			
			if (MenuItem("Open in Explorer")) {
				std::string command = "explorer " + m_CurrentDirectory.string();
				system(command.c_str());
			}
			if (MenuItem("Refresh")) {
				RefreshAllThumbnails();
			}
			
			EndPopup();
		}
		
		// Create Folder Dialog
		if (m_ShowCreateFolderDialog) {
			OpenPopup("Create New Folder");
			m_ShowCreateFolderDialog = false;
		}
		
		CenterNextWindow();
		
		if (BeginModal("Create New Folder", nullptr, Size(400, 150))) {
			TextStyled("Enter folder name:");
			AddSpacing(SpacingValues::SM);
			
			ImGui::SetNextItemWidth(300);
			if (ImGui::InputText("##FolderName", m_NewItemName, sizeof(m_NewItemName), ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::filesystem::path newFolderPath = m_CurrentDirectory / m_NewItemName;
				if (!std::filesystem::exists(newFolderPath)) {
					std::filesystem::create_directory(newFolderPath);
					LNX_LOG_INFO("Created folder: {0}", newFolderPath.string());
				}
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}
			
			AddSpacing(SpacingValues::SM);
			
			if (Button("Create", ButtonVariant::Primary, ButtonSize::Medium, Size(145, 0))) {
				std::filesystem::path newFolderPath = m_CurrentDirectory / m_NewItemName;
				if (!std::filesystem::exists(newFolderPath)) {
					std::filesystem::create_directory(newFolderPath);
					LNX_LOG_INFO("Created folder: {0}", newFolderPath.string());
				}
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}
			
			SameLine();
			
			if (Button("Cancel", ButtonVariant::Default, ButtonSize::Medium, Size(145, 0))) {
				strcpy_s(m_NewItemName, "NewFolder");
				ImGui::CloseCurrentPopup();
			}
			
			EndModal();
		}
		
		// Rename Dialog
		if (m_ShowRenameDialog) {
			OpenPopup("Rename Item");
			m_ShowRenameDialog = false;
		}
		
		if (BeginModal("Rename Item", nullptr, Size(400, 150))) {
			TextStyled("Enter new name:");
			AddSpacing(SpacingValues::SM);
			
			ImGui::SetNextItemWidth(300);
			if (ImGui::InputText("##ItemName", m_NewItemName, sizeof(m_NewItemName), ImGuiInputTextFlags_EnterReturnsTrue)) {
				RenameItem(m_ItemToRename);
				ImGui::CloseCurrentPopup();
			}
			
			AddSpacing(SpacingValues::SM);
			
			if (Button("Rename", ButtonVariant::Primary, ButtonSize::Medium, Size(145, 0))) {
				RenameItem(m_ItemToRename);
				ImGui::CloseCurrentPopup();
			}
			
			SameLine();
			
			if (Button("Cancel", ButtonVariant::Default, ButtonSize::Medium, Size(145, 0))) {
				ImGui::CloseCurrentPopup();
			}
			
			EndModal();
		}
	}

	void ContentBrowserPanel::RenderItemContextMenu(const std::filesystem::path& path) {
		using namespace UI;
		
		std::string filename = path.filename().string();
		bool isDirectory = std::filesystem::is_directory(path);
		
		if (m_SelectedItems.size() > 1) {
			ImGui::Text("%d items selected", (int)m_SelectedItems.size());
		} else {
			ImGui::Text("%s", filename.c_str());
		}
		
		Separator();
		
		// Specific options for .lumesh files
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lumesh" && m_SelectedItems.size() == 1) {
			if (MenuItem("Create Prefab")) {
				CreatePrefabFromMesh(path);
			}
			Separator();
		}
		
		if (m_SelectedItems.size() == 1) {
			if (MenuItem("Rename")) {
				m_ItemToRename = path;
				strncpy_s(m_NewItemName, sizeof(m_NewItemName), filename.c_str(), _TRUNCATE);
				m_ShowRenameDialog = true;
			}
		}
		
		if (MenuItem("Delete")) {
			for (const auto& selectedPath : m_SelectedItems) {
				DeleteItem(std::filesystem::path(selectedPath));
			}
			ClearSelection();
			m_ContextMenuTargetItem.clear();
		}
		
		if (m_SelectedItems.size() == 1) {
			Separator();
			
			if (MenuItem("Show in Explorer")) {
				std::string command = "explorer /select," + path.string();
				system(command.c_str());
			}
			
			if (isDirectory) {
				if (MenuItem("Open in File Explorer")) {
					std::string command = "explorer " + path.string();
					system(command.c_str());
				}
			}
		}
	}

	// ============================================================================
	// FILE GRID CALLBACKS
	// ============================================================================

	void ContentBrowserPanel::OnFileGridItemClicked(const std::filesystem::path& path) {
		bool ctrlDown = ImGui::GetIO().KeyCtrl;
		bool shiftDown = ImGui::GetIO().KeyShift;
		
		if (ctrlDown) {
			ToggleSelection(path);
			m_LastSelectedItem = path;
		}
		else if (shiftDown && !m_LastSelectedItem.empty()) {
			SelectRange(m_LastSelectedItem, path);
			m_LastSelectedItem = path;
		}
		else {
			if (!IsSelected(path)) {
				ClearSelection();
				AddToSelection(path);
			}
			m_LastSelectedItem = path;
		}
	}

	void ContentBrowserPanel::OnFileGridItemDoubleClicked(const std::filesystem::path& path) {
		if (std::filesystem::is_directory(path)) {
			AddToHistory(path);
			m_CurrentDirectory = path;
		}
		else {
			std::string ext = path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			
			if (ext == ".lumat") {
				if (m_OnMaterialOpenCallback) {
					m_OnMaterialOpenCallback(path);
				}
				else {
					LNX_LOG_WARN("Material editor not connected - cannot open {0}", path.filename().string());
				}
			}
			else if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") {
				std::string command = "cmd /c start \"\" \"" + path.string() + "\"";
				system(command.c_str());
				LNX_LOG_INFO("Opening script in editor: {0}", path.filename().string());
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

	void ContentBrowserPanel::OnFileGridItemRightClicked(const std::filesystem::path& path) {
		if (!IsSelected(path)) {
			ClearSelection();
			AddToSelection(path);
		}
		m_ContextMenuTargetItem = path;
		m_OpenItemContextMenu = true;
	}

	void ContentBrowserPanel::OnFileGridItemDropped(const std::filesystem::path& target, const void* data, size_t size) {
		ContentBrowserPayload* payload = (ContentBrowserPayload*)data;
		std::filesystem::path sourcePath(payload->FilePath);
		std::filesystem::path destPath = target / sourcePath.filename();
		
		try {
			if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
				std::filesystem::rename(sourcePath, destPath);
				LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(), target.filename().string());
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to move {0}: {1}", sourcePath.filename().string(), e.what());
		}
	}

	void ContentBrowserPanel::OnFileGridMultiItemDropped(const std::filesystem::path& target, const std::string& data) {
		std::stringstream ss(data);
		std::string line;
		
		while (std::getline(ss, line)) {
			if (line.empty()) continue;
			
			std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
			std::filesystem::path destPath = target / sourcePath.filename();
			
			try {
				if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
					std::filesystem::rename(sourcePath, destPath);
					LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(), target.filename().string());
				}
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to move {0}: {1}", sourcePath.filename().string(), e.what());
			}
		}
		
		ClearSelection();
	}

	// ============================================================================
	// DIRECTORY TREE CALLBACKS
	// ============================================================================

	void ContentBrowserPanel::OnDirectorySelected(const std::filesystem::path& path) {
		if (m_CurrentDirectory != path) {
			AddToHistory(path);
			m_CurrentDirectory = path;
		}
	}

	void ContentBrowserPanel::OnDirectorySingleItemDropped(const std::filesystem::path& target, const void* data, size_t size) {
		ContentBrowserPayload* payload = (ContentBrowserPayload*)data;
		std::filesystem::path sourcePath(payload->FilePath);
		std::filesystem::path destPath = target / sourcePath.filename();
		
		try {
			if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
				std::filesystem::rename(sourcePath, destPath);
				LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(), target.filename().string());
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to move {0}: {1}", sourcePath.filename().string(), e.what());
		}
	}

	void ContentBrowserPanel::OnDirectoryMultiItemDropped(const std::filesystem::path& target, const std::string& data) {
		std::stringstream ss(data);
		std::string line;
		
		while (std::getline(ss, line)) {
			if (line.empty()) continue;
			
			std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
			std::filesystem::path destPath = target / sourcePath.filename();
			
			try {
				if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
					std::filesystem::rename(sourcePath, destPath);
					LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(), target.filename().string());
				}
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to move {0}: {1}", sourcePath.filename().string(), e.what());
			}
		}
		
		ClearSelection();
	}

	// ============================================================================
	// ICONS & THUMBNAILS
	// ============================================================================

	Ref<Texture2D> ContentBrowserPanel::GetIconForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lunex") return m_SceneIcon ? m_SceneIcon : m_FileIcon;
		if (extension == ".lumat") return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		if (extension == ".lumesh") return m_MeshIcon ? m_MeshIcon : m_FileIcon;
		if (extension == ".luprefab") return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
			return m_TextureIcon ? m_TextureIcon : m_FileIcon;
		if (extension == ".glsl" || extension == ".shader")
			return m_ShaderIcon ? m_ShaderIcon : m_FileIcon;
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
			return m_AudioIcon ? m_AudioIcon : m_FileIcon;
		if (extension == ".cpp" || extension == ".h" || extension == ".cs")
			return m_ScriptIcon ? m_ScriptIcon : m_FileIcon;

		return m_FileIcon;
	}

	Ref<Texture2D> ContentBrowserPanel::GetThumbnailForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		// HDR/HDRI files
		if (extension == ".hdr" || extension == ".hdri") {
			std::string pathStr = path.string();

			auto it = m_TextureCache.find(pathStr);
			if (it != m_TextureCache.end() && it->second) {
				return it->second;
			}

			try {
				Ref<Texture2D> hdrTexture = Texture2D::Create(pathStr);
				if (hdrTexture && hdrTexture->IsLoaded()) {
					m_TextureCache[pathStr] = hdrTexture;
					return hdrTexture;
				}
			}
			catch (...) {
				LNX_LOG_WARN("Failed to load HDR thumbnail: {0}", path.filename().string());
			}

			return m_TextureIcon ? m_TextureIcon : m_FileIcon;
		}

		// Material files
		if (extension == ".lumat") {
			std::string pathStr = path.string();

			auto it = m_MaterialThumbnailCache.find(pathStr);
			if (it != m_MaterialThumbnailCache.end() && it->second) {
				return it->second;
			}

			try {
				auto material = MaterialRegistry::Get().LoadMaterial(path);
				if (material && m_MaterialPreviewRenderer) {
					Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->GetOrGenerateCachedThumbnail(path, material);
					if (thumbnail) {
						m_MaterialThumbnailCache[pathStr] = thumbnail;
						return thumbnail;
					}
				}
			}
			catch (...) {}

			return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		}

		// Mesh Asset files
		if (extension == ".lumesh") {
			std::string pathStr = path.string();

			auto it = m_MeshThumbnailCache.find(pathStr);
			if (it != m_MeshThumbnailCache.end() && it->second) {
				return it->second;
			}

			try {
				auto meshAsset = MeshAsset::LoadFromFile(path);
				if (meshAsset && meshAsset->GetModel() && m_MaterialPreviewRenderer) {
					Ref<Model> originalModel = m_MaterialPreviewRenderer->GetPreviewModel();
					m_MaterialPreviewRenderer->SetPreviewModel(meshAsset->GetModel());

					auto originalCameraPos = m_MaterialPreviewRenderer->GetCameraPosition();
					auto originalBgColor = m_MaterialPreviewRenderer->GetBackgroundColor();

					// Compute camera position from model bounds for proper centering
					glm::vec3 boundsMin = meshAsset->GetBoundsMin();
					glm::vec3 boundsMax = meshAsset->GetBoundsMax();
					// If metadata bounds are zero (not computed), compute from model directly
					if (glm::length(boundsMax - boundsMin) < 0.001f) {
						MaterialPreviewRenderer::ComputeModelBounds(meshAsset->GetModel(), boundsMin, boundsMax);
					}
					glm::vec3 cameraPos = MaterialPreviewRenderer::ComputeCameraForBounds(boundsMin, boundsMax);
					m_MaterialPreviewRenderer->SetCameraPosition(cameraPos);
					m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f));

					auto defaultMaterial = CreateRef<MaterialAsset>("MeshPreview");
					defaultMaterial->SetAlbedo(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
					defaultMaterial->SetMetallic(0.0f);
					defaultMaterial->SetRoughness(0.5f);

					Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->RenderToTexture(defaultMaterial);

					m_MaterialPreviewRenderer->SetCameraPosition(originalCameraPos);
					m_MaterialPreviewRenderer->SetBackgroundColor(originalBgColor);
					m_MaterialPreviewRenderer->SetPreviewModel(originalModel);

					if (thumbnail) {
						m_MeshThumbnailCache[pathStr] = thumbnail;
						return thumbnail;
					}
				}
			}
			catch (...) {}

			return m_MeshIcon ? m_MeshIcon : m_FileIcon;
		}

		// Prefab files
		if (extension == ".luprefab") {
			std::string pathStr = path.string();

			auto it = m_MeshThumbnailCache.find(pathStr);
			if (it != m_MeshThumbnailCache.end() && it->second) {
				return it->second;
			}

			try {
				auto prefab = Prefab::LoadFromFile(path);
				if (prefab && m_MaterialPreviewRenderer) {
					for (const auto& entityData : prefab->GetEntityData()) {
						for (const auto& compData : entityData.Components) {
							if (compData.ComponentType == "MeshComponent") {
								std::istringstream iss(compData.SerializedData);
								std::string token;

								std::getline(iss, token, ';');
								std::getline(iss, token, ';');
								std::getline(iss, token, ';');
								std::getline(iss, token, ';');
								std::string meshAssetPath = token;

								if (!meshAssetPath.empty()) {
									std::filesystem::path fullMeshPath = m_BaseDirectory / meshAssetPath;
									if (std::filesystem::exists(fullMeshPath)) {
										auto meshAsset = MeshAsset::LoadFromFile(fullMeshPath);
										if (meshAsset && meshAsset->GetModel()) {
											Ref<Model> originalModel = m_MaterialPreviewRenderer->GetPreviewModel();
											m_MaterialPreviewRenderer->SetPreviewModel(meshAsset->GetModel());

											auto originalCameraPos = m_MaterialPreviewRenderer->GetCameraPosition();
											auto originalBgColor = m_MaterialPreviewRenderer->GetBackgroundColor();

											// Compute camera position from model bounds for proper centering
											glm::vec3 boundsMin = meshAsset->GetBoundsMin();
											glm::vec3 boundsMax = meshAsset->GetBoundsMax();
											if (glm::length(boundsMax - boundsMin) < 0.001f) {
												MaterialPreviewRenderer::ComputeModelBounds(meshAsset->GetModel(), boundsMin, boundsMax);
											}
											glm::vec3 cameraPos = MaterialPreviewRenderer::ComputeCameraForBounds(boundsMin, boundsMax);
											m_MaterialPreviewRenderer->SetCameraPosition(cameraPos);
											m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f));

											auto defaultMaterial = CreateRef<MaterialAsset>("PrefabPreview");
											defaultMaterial->SetAlbedo(glm::vec4(0.6f, 0.65f, 0.7f, 1.0f));
											defaultMaterial->SetMetallic(0.0f);
											defaultMaterial->SetRoughness(0.4f);

											Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->RenderToTexture(defaultMaterial);

											m_MaterialPreviewRenderer->SetCameraPosition(originalCameraPos);
											m_MaterialPreviewRenderer->SetBackgroundColor(originalBgColor);
											m_MaterialPreviewRenderer->SetPreviewModel(originalModel);

											if (thumbnail) {
												m_MeshThumbnailCache[pathStr] = thumbnail;
												return thumbnail;
											}
										}
									}
								}
								break;
							}
						}
					}
				}
			}
			catch (...) {}

			return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
		}

		// Image files
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".TGA" || extension == ".tga") {
			std::string pathStr = path.string();

			auto it = m_TextureCache.find(pathStr);
			if (it != m_TextureCache.end()) {
				return it->second;
			}

			try {
				Ref<Texture2D> texture = Texture2D::Create(pathStr);
				if (texture) {
					m_TextureCache[pathStr] = texture;
					return texture;
				}
			}
			catch (...) {}
		}

		return GetIconForFile(path);
	}

	std::string ContentBrowserPanel::GetAssetTypeLabel(const std::filesystem::path& path) {
		if (std::filesystem::is_directory(path)) {
			return "FOLDER";
		}

		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lumat") return "MATERIAL";
		if (extension == ".lumesh") return "MESH";
		if (extension == ".luprefab") return "PREFAB";
		if (extension == ".lunex") return "SCENE";
		if (extension == ".hdr" || extension == ".hdri" || extension == ".exr") return "HDRI";
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".TGA" || extension == ".tga") return "TEXTURE";
		if (extension == ".glsl" || extension == ".shader") return "SHADER";
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") return "AUDIO";
		if (extension == ".cpp" || extension == ".h" || extension == ".cs") return "SCRIPT";

		return "FILE";
	}

	// ============================================================================
	// EVENT HANDLING
	// ============================================================================

	void ContentBrowserPanel::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<FileDropEvent>([this](FileDropEvent& event) {
			if (m_IsHovered) {
				if (!m_HoveredFolder.empty() && std::filesystem::exists(m_HoveredFolder)) {
					ImportFilesToFolder(event.GetPaths(), m_HoveredFolder);
				}
				else {
					ImportFiles(event.GetPaths());
				}
				return true;
			}
			return false;
		});
	}

	// ============================================================================
	// FILE OPERATIONS
	// ============================================================================

	void ContentBrowserPanel::CreateNewFolder() {
		m_ShowCreateFolderDialog = true;
		strcpy_s(m_NewItemName, "NewFolder");
	}

	void ContentBrowserPanel::CreateNewScene() {
		int counter = 1;
		std::filesystem::path scenePath;
		do {
			std::string sceneName = "NewScene" + (counter > 1 ? std::to_string(counter) : "") + ".lunex";
			scenePath = m_CurrentDirectory / sceneName;
			counter++;
		} while (std::filesystem::exists(scenePath));

		std::string sceneContent = R"(Scene: NewScene
Entities:
)";

		std::ofstream sceneFile(scenePath);
		sceneFile << sceneContent;
		sceneFile.close();

		LNX_LOG_INFO("Created new scene: {0}", scenePath.string());
	}

	void ContentBrowserPanel::CreateNewScript() {
		int counter = 1;
		std::filesystem::path scriptPath;
		std::string baseName;
		do {
			baseName = "NewScript" + (counter > 1 ? std::to_string(counter) : "");
			std::string scriptName = baseName + ".cpp";
			scriptPath = m_CurrentDirectory / scriptName;
			counter++;
		} while (std::filesystem::exists(scriptPath));

		std::string scriptContent = R"(#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include <iostream>

namespace Lunex {

    class )" + baseName + R"( : public IScriptModule
    {
    public:
        )" + baseName + R"(() = default;
        ~)" + baseName + R"(() override = default;

        void OnLoad(EngineContext* context) override
        {
            m_Context = context;
            
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Script loaded!");
            }
        }

        void OnUnload() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Script unloading...");
            }
            m_Context = nullptr;
        }

        void OnUpdate(float deltaTime) override
        {
        }

        void OnRender() override
        {
        }

        void OnPlayModeEnter() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Entering Play Mode!");
            }
        }

        void OnPlayModeExit() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Exiting Play Mode!");
            }
        }

    private:
        EngineContext* m_Context = nullptr;
    };

} // namespace Lunex

extern "C"
{
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion()
    {
        return Lunex::SCRIPTING_API_VERSION;
    }

    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule()
    {
        return new Lunex::)" + baseName + R"(();
    }

    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module)
    {
        delete module;
    }
}
)";

		std::ofstream scriptFile(scriptPath);
		scriptFile << scriptContent;
		scriptFile.close();

		LNX_LOG_INFO("Created new script: {0}", scriptPath.string());
	}

	void ContentBrowserPanel::CreateNewMaterial() {
		int counter = 1;
		std::filesystem::path materialPath;
		std::string baseName;
		do {
			baseName = "NewMaterial" + (counter > 1 ? std::to_string(counter) : "");
			std::string materialName = baseName + ".lumat";
			materialPath = m_CurrentDirectory / materialName;
			counter++;
		} while (std::filesystem::exists(materialPath));

		UUID materialID = UUID();

		std::stringstream ss;
		ss << "Material:\n";
		ss << "  ID: " << (uint64_t)materialID << "\n";
		ss << "  Name: " << baseName << "\n";
		ss << "Properties:\n";
		ss << "  Albedo: [1, 1, 1, 1]\n";
		ss << "  Metallic: 0\n";
		ss << "  Roughness: 0.5\n";
		ss << "  Specular: 0.5\n";
		ss << "  EmissionColor: [0, 0, 0]\n";
		ss << "  EmissionIntensity: 0\n";
		ss << "  NormalIntensity: 1\n";
		ss << "Textures:\n";
		ss << "Multipliers:\n";
		ss << "  Metallic: 1\n";
		ss << "  Roughness: 1\n";
		ss << "  Specular: 1\n";
		ss << "  AO: 1\n";

		std::ofstream materialFile(materialPath);
		materialFile << ss.str();
		materialFile.close();

		LNX_LOG_INFO("Created new material: {0}", materialPath.string());
	}

	void ContentBrowserPanel::CreatePrefabFromMesh(const std::filesystem::path& meshAssetPath) {
		auto meshAsset = MeshAsset::LoadFromFile(meshAssetPath);
		if (!meshAsset) {
			LNX_LOG_ERROR("Failed to load mesh asset: {0}", meshAssetPath.string());
			return;
		}

		std::string baseName = meshAssetPath.stem().string();
		std::filesystem::path prefabsFolder = m_BaseDirectory / "Prefabs";

		if (!std::filesystem::exists(prefabsFolder)) {
			std::filesystem::create_directories(prefabsFolder);
			LNX_LOG_INFO("Created Prefabs folder: {0}", prefabsFolder.string());
		}

		int counter = 1;
		std::filesystem::path prefabPath;
		std::string prefabName = baseName;
		do {
			prefabName = baseName + (counter > 1 ? std::to_string(counter) : "");
			std::string prefabFilename = prefabName + ".luprefab";
			prefabPath = prefabsFolder / prefabFilename;
			counter++;
		} while (std::filesystem::exists(prefabPath));

		UUID prefabID = UUID();
		UUID entityID = UUID();

		auto relativeMeshPath = std::filesystem::relative(meshAssetPath, g_AssetPath);
		std::string relativeMeshPathStr = relativeMeshPath.string();
		std::replace(relativeMeshPathStr.begin(), relativeMeshPathStr.end(), '\\', '/');

		std::stringstream ss;
		ss << "Prefab:\n";
		ss << "  Name: " << prefabName << "\n";
		ss << "  Description: Prefab created from mesh " << baseName << "\n";
		ss << "  RootEntityID: " << (uint64_t)entityID << "\n";
		ss << "  UUID: " << (uint64_t)prefabID << "\n";
		ss << "  OriginalTransform:\n";
		ss << "    Position: [0, 0, 0]\n";
		ss << "    Rotation: [0, 0, 0]\n";
		ss << "    Scale: [1, 1, 1]\n";
		ss << "Entities:\n";
		ss << "  - EntityID: " << (uint64_t)entityID << "\n";
		ss << "    Tag: " << prefabName << "\n";
		ss << "    LocalParentID: 0\n";
		ss << "    LocalChildIDs: []\n";
		ss << "    Components:\n";
		ss << "      - Type: TransformComponent\n";
		ss << "        Data: \"0,0,0;0,0,0;1,1,1\"\n";
		ss << "      - Type: MeshComponent\n";
		ss << "        Data: \"4;1,1,1,1;" << (uint64_t)meshAsset->GetID() << ";" << relativeMeshPathStr << ";\"\n";
		ss << "      - Type: MaterialComponent\n";
		ss << "        Data: \"0;;0;1,1,1,1;0;0.5;0.5;0,0,0;0\"\n";

		std::ofstream prefabFile(prefabPath);
		if (!prefabFile) {
			LNX_LOG_ERROR("Failed to create prefab file: {0}", prefabPath.string());
			return;
		}
		prefabFile << ss.str();
		prefabFile.close();

		LNX_LOG_INFO("Created prefab '{0}' from mesh '{1}'", prefabName, baseName);
	}

	void ContentBrowserPanel::DeleteItem(const std::filesystem::path& path) {
		try {
			if (std::filesystem::is_directory(path)) {
				std::filesystem::remove_all(path);
				LNX_LOG_INFO("Deleted folder: {0}", path.string());
			}
			else {
				std::filesystem::remove(path);
				LNX_LOG_INFO("Deleted file: {0}", path.string());
			}

			auto it = m_TextureCache.find(path.string());
			if (it != m_TextureCache.end()) {
				m_TextureCache.erase(it);
			}
			
			ProjectManager::RefreshAssetDatabase();
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to delete {0}: {1}", path.string(), e.what());
		}
	}

	void ContentBrowserPanel::RenameItem(const std::filesystem::path& oldPath) {
		try {
			std::filesystem::path newPath = oldPath.parent_path() / m_NewItemName;

			if (std::filesystem::exists(newPath)) {
				LNX_LOG_WARN("Item with name {0} already exists", m_NewItemName);
				return;
			}

			std::string oldExtension = oldPath.extension().string();
			std::transform(oldExtension.begin(), oldExtension.end(), oldExtension.begin(), ::tolower);

			if (oldExtension == ".lumat") {
				auto material = MaterialRegistry::Get().LoadMaterial(oldPath);
				if (material) {
					std::filesystem::path newNamePath(m_NewItemName);
					std::string newMaterialName = newNamePath.stem().string();

					material->SetName(newMaterialName);
					material->SetPath(newPath);

					if (!material->SaveToFile(newPath)) {
						LNX_LOG_ERROR("Failed to save material with new name: {0}", m_NewItemName);
						return;
					}

					std::filesystem::remove(oldPath);
					MaterialRegistry::Get().ReloadMaterial(material->GetID());

					LNX_LOG_INFO("Renamed material {0} to {1}", oldPath.filename().string(), newMaterialName);
				}
				else {
					LNX_LOG_ERROR("Failed to load material for renaming: {0}", oldPath.string());
					return;
				}
			}
			else {
				std::filesystem::rename(oldPath, newPath);
				LNX_LOG_INFO("Renamed {0} to {1}", oldPath.filename().string(), m_NewItemName);
			}

			auto it = m_TextureCache.find(oldPath.string());
			if (it != m_TextureCache.end()) {
				auto texture = it->second;
				m_TextureCache.erase(it);
				m_TextureCache[newPath.string()] = texture;
			}
			
			ProjectManager::RefreshAssetDatabase();
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to rename {0}: {1}", oldPath.string(), e.what());
		}
	}

	void ContentBrowserPanel::DuplicateItem(const std::filesystem::path& path) {
		try {
			std::string baseName = path.stem().string();
			std::string extension = path.extension().string();
			std::filesystem::path destPath;
			int counter = 1;

			do {
				std::string newName = baseName + " - Copy";
				if (counter > 1) {
					newName += " (" + std::to_string(counter) + ")";
				}
				newName += extension;
				destPath = path.parent_path() / newName;
				counter++;
			} while (std::filesystem::exists(destPath));

			if (std::filesystem::is_directory(path)) {
				std::filesystem::copy(path, destPath, std::filesystem::copy_options::recursive);
			}
			else {
				std::filesystem::copy_file(path, destPath);
			}

			LNX_LOG_INFO("Duplicated {0} as {1}", path.filename().string(), destPath.filename().string());
			ProjectManager::RefreshAssetDatabase();
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to duplicate {0}: {1}", path.filename().string(), e.what());
		}
	}

	void ContentBrowserPanel::ImportFiles(const std::vector<std::string>& files) {
		for (const auto& file : files) {
			try {
				std::filesystem::path sourcePath(file);
				std::filesystem::path destPath = m_CurrentDirectory / sourcePath.filename();

				if (std::filesystem::exists(destPath)) {
					LNX_LOG_WARN("File {0} already exists in destination", sourcePath.filename().string());
					continue;
				}

				std::filesystem::copy(sourcePath, destPath);
				LNX_LOG_INFO("Imported file: {0}", sourcePath.filename().string());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to import {0}: {1}", file, e.what());
			}
		}
		ProjectManager::RefreshAssetDatabase();
	}

	void ContentBrowserPanel::ImportFilesToFolder(const std::vector<std::string>& files, const std::filesystem::path& targetFolder) {
		for (const auto& file : files) {
			try {
				std::filesystem::path sourcePath(file);
				std::filesystem::path destPath = targetFolder / sourcePath.filename();

				if (std::filesystem::exists(destPath)) {
					LNX_LOG_WARN("File {0} already exists in {1}", sourcePath.filename().string(), targetFolder.filename().string());
					continue;
				}

				std::filesystem::copy(sourcePath, destPath);
				LNX_LOG_INFO("Imported {0} to folder {1}", sourcePath.filename().string(), targetFolder.filename().string());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to import {0}: {1}", file, e.what());
			}
		}
		ProjectManager::RefreshAssetDatabase();
	}

	// ============================================================================
	// CACHE MANAGEMENT
	// ============================================================================

	void ContentBrowserPanel::InvalidateMaterialThumbnail(const std::filesystem::path& materialPath) {
		std::string pathStr = materialPath.string();
		auto it = m_MaterialThumbnailCache.find(pathStr);
		if (it != m_MaterialThumbnailCache.end()) {
			m_MaterialThumbnailCache.erase(it);
			LNX_LOG_INFO("Invalidated thumbnail cache for: {0}", materialPath.filename().string());
		}
	}

	void ContentBrowserPanel::InvalidateThumbnailDiskCache(const std::filesystem::path& materialPath) {
		if (m_MaterialPreviewRenderer) {
			m_MaterialPreviewRenderer->InvalidateCachedThumbnail(materialPath);
		}
	}

	void ContentBrowserPanel::RefreshAllThumbnails() {
		m_TextureCache.clear();
		m_MaterialThumbnailCache.clear();
		m_MeshThumbnailCache.clear();
		LNX_LOG_INFO("Refreshed all Content Browser thumbnails");
	}

	// ============================================================================
	// SELECTION
	// ============================================================================

	bool ContentBrowserPanel::IsSelected(const std::filesystem::path& path) const {
		return m_SelectedItems.find(path.string()) != m_SelectedItems.end();
	}

	void ContentBrowserPanel::AddToSelection(const std::filesystem::path& path) {
		m_SelectedItems.insert(path.string());
		m_LastSelectedItem = path;
	}

	void ContentBrowserPanel::RemoveFromSelection(const std::filesystem::path& path) {
		m_SelectedItems.erase(path.string());
	}

	void ContentBrowserPanel::ToggleSelection(const std::filesystem::path& path) {
		if (IsSelected(path)) {
			RemoveFromSelection(path);
		}
		else {
			AddToSelection(path);
		}
	}

	void ContentBrowserPanel::SelectRange(const std::filesystem::path& from, const std::filesystem::path& to) {
		std::vector<std::filesystem::path> items;
		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			items.push_back(entry.path());
		}

		int fromIndex = -1;
		int toIndex = -1;
		for (int i = 0; i < items.size(); i++) {
			if (items[i] == from) fromIndex = i;
			if (items[i] == to) toIndex = i;
		}

		if (fromIndex != -1 && toIndex != -1) {
			int start = (std::min)(fromIndex, toIndex);
			int end = (std::max)(fromIndex, toIndex);

			for (int i = start; i <= end; i++) {
				AddToSelection(items[i]);
			}
		}
	}

	void ContentBrowserPanel::ClearSelection() {
		m_SelectedItems.clear();
		m_LastSelectedItem.clear();
	}

	void ContentBrowserPanel::SelectAll() {
		ClearSelection();

		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			if (MatchesSearchFilter(entry.path().filename().string())) {
				AddToSelection(entry.path());
			}
		}

		LNX_LOG_INFO("Selected all {0} items", m_SelectedItems.size());
	}

	// ============================================================================
	// PUBLIC API FOR GLOBAL SHORTCUTS
	// ============================================================================

	void ContentBrowserPanel::DeleteSelectedItems() {
		for (const auto& selectedPath : m_SelectedItems) {
			DeleteItem(std::filesystem::path(selectedPath));
		}
		ClearSelection();
	}

	void ContentBrowserPanel::DuplicateSelectedItem() {
		if (m_SelectedItems.size() == 1) {
			DuplicateItem(std::filesystem::path(*m_SelectedItems.begin()));
		}
	}

	void ContentBrowserPanel::RenameSelectedItem() {
		if (m_SelectedItems.size() == 1) {
			m_ItemToRename = std::filesystem::path(*m_SelectedItems.begin());
			strncpy_s(m_NewItemName, sizeof(m_NewItemName),
				m_ItemToRename.filename().string().c_str(), _TRUNCATE);
			m_ShowRenameDialog = true;
		}
	}

	// ============================================================================
	// NAVIGATION
	// ============================================================================

	void ContentBrowserPanel::NavigateBack() {
		if (m_HistoryIndex > 0) {
			m_HistoryIndex--;
			m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
		}
	}

	void ContentBrowserPanel::NavigateForward() {
		if (m_HistoryIndex < (int)m_DirectoryHistory.size() - 1) {
			m_HistoryIndex++;
			m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
		}
	}

	void ContentBrowserPanel::NavigateUp() {
		NavigateToParent();
	}

	void ContentBrowserPanel::NavigateToParent() {
		if (m_CurrentDirectory == m_BaseDirectory) return;

		std::filesystem::path parentPath = m_CurrentDirectory.parent_path();
		NavigateToDirectory(parentPath);
	}

	void ContentBrowserPanel::NavigateToDirectory(const std::filesystem::path& directory) {
		if (m_CurrentDirectory == directory) return;

		AddToHistory(directory);
		m_CurrentDirectory = directory;
		ClearSelection();
	}

	void ContentBrowserPanel::AddToHistory(const std::filesystem::path& directory) {
		m_HistoryIndex++;
		if (m_HistoryIndex < (int)m_DirectoryHistory.size()) {
			m_DirectoryHistory.erase(
				m_DirectoryHistory.begin() + m_HistoryIndex,
				m_DirectoryHistory.end()
			);
		}
		m_DirectoryHistory.push_back(directory);
	}

	// ============================================================================
	// CLIPBOARD OPERATIONS
	// ==============================================================================

	void ContentBrowserPanel::CopySelectedItems() {
		m_ClipboardOperation = ClipboardOperation::Copy;
		m_ClipboardItems.clear();

		for (const auto& selectedPath : m_SelectedItems) {
			m_ClipboardItems.push_back(std::filesystem::path(selectedPath));
		}

		LNX_LOG_INFO("Copied {0} item(s) to clipboard", m_ClipboardItems.size());
	}

	void ContentBrowserPanel::CutSelectedItems() {
		m_ClipboardOperation = ClipboardOperation::Cut;
		m_ClipboardItems.clear();

		for (const auto& selectedPath : m_SelectedItems) {
			m_ClipboardItems.push_back(std::filesystem::path(selectedPath));
		}

		LNX_LOG_INFO("Cut {0} item(s) to clipboard", m_ClipboardItems.size());
	}

	void ContentBrowserPanel::PasteItems() {
		if (!CanPaste()) return;

		for (const auto& sourcePath : m_ClipboardItems) {
			try {
				std::filesystem::path destPath = m_CurrentDirectory / sourcePath.filename();

				if (std::filesystem::exists(destPath)) {
					std::string baseName = sourcePath.stem().string();
					std::string extension = sourcePath.extension().string();
					int counter = 1;

					do {
						std::string newName = baseName + " (" + std::to_string(counter) + ")";
						newName += extension;
						destPath = m_CurrentDirectory / newName;
						counter++;
					} while (std::filesystem::exists(destPath));
				}

				if (m_ClipboardOperation == ClipboardOperation::Copy) {
					if (std::filesystem::is_directory(sourcePath)) {
						std::filesystem::copy(sourcePath, destPath, std::filesystem::copy_options::recursive);
					}
					else {
						std::filesystem::copy_file(sourcePath, destPath);
					}
					LNX_LOG_INFO("Copied {0} to {1}", sourcePath.filename().string(), destPath.filename().string());
				}
				else if (m_ClipboardOperation == ClipboardOperation::Cut) {
					std::filesystem::rename(sourcePath, destPath);
					LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(), destPath.filename().string());
				}
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to paste {0}: {1}", sourcePath.filename().string(), e.what());
			}
		}

		if (m_ClipboardOperation == ClipboardOperation::Cut) {
			m_ClipboardItems.clear();
			m_ClipboardOperation = ClipboardOperation::None;
		}

		ClearSelection();
		ProjectManager::RefreshAssetDatabase();
	}

	bool ContentBrowserPanel::CanPaste() const {
		return !m_ClipboardItems.empty() && m_ClipboardOperation != ClipboardOperation::None;
	}

	// ============================================================================
	// UTILITIES
	// ============================================================================

	std::string ContentBrowserPanel::GetFileSizeString(uintmax_t size) {
		const char* units[] = { "B", "KB", "MB", "GB", "TB" };
		int unitIndex = 0;
		double displaySize = (double)size;

		while (displaySize >= 1024.0 && unitIndex < 4) {
			displaySize /= 1024.0;
			unitIndex++;
		}

		char buffer[64];
		if (unitIndex == 0) {
			snprintf(buffer, sizeof(buffer), "%d %s", (int)displaySize, units[unitIndex]);
		}
		else {
			snprintf(buffer, sizeof(buffer), "%.2f %s", displaySize, units[unitIndex]);
		}

		return std::string(buffer);
	}

	std::string ContentBrowserPanel::GetLastModifiedString(const std::filesystem::path& path) {
		auto ftime = std::filesystem::last_write_time(path);
		auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
			ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
		);
		std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

		char buffer[64];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&cftime));

		return std::string(buffer);
	}
}