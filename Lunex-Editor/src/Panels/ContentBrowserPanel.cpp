#include "stpch.h"
#include "ContentBrowserPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

namespace Lunex {

	const std::filesystem::path g_AssetPath = "assets";

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(g_AssetPath)
	{
		// Load icons
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/DirectoryIcon.png");
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
		m_BackIcon = Texture2D::Create("Resources/Icons/ContentBrowser/BackIcon.png");
		m_ForwardIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ForwardIcon.png");
		m_RefreshIcon = Texture2D::Create("Resources/Icons/ContentBrowser/RefreshIcon.png");
		m_FavoriteIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FavoriteIcon.png");

		// Initialize navigation history
		m_NavigationHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;

		// Add default favorites
		if (std::filesystem::exists(g_AssetPath / "Textures"))
			m_Favorites.push_back(g_AssetPath / "Textures");
		if (std::filesystem::exists(g_AssetPath / "Models"))
			m_Favorites.push_back(g_AssetPath / "Models");
		if (std::filesystem::exists(g_AssetPath / "Scenes"))
			m_Favorites.push_back(g_AssetPath / "Scenes");
	}

	void ContentBrowserPanel::OnImGuiRender() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Content Browser");
		ImGui::PopStyleVar();

		// Top toolbar with dark background
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
		ImGui::BeginChild("Toolbar", ImVec2(0, 40), true, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleColor();

		RenderTopBar();
		ImGui::EndChild();

		// Main content area with splitters
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		static float leftPanelWidth = m_FavoritesPanelWidth;
		static float rightPanelWidth = m_PreviewPanelWidth;

		// Left panel (Favorites)
		if (m_ShowFavorites) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
			ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0), true);
			ImGui::PopStyleColor();
			RenderFavoritesPanel();
			ImGui::EndChild();

			ImGui::SameLine();

			// Vertical splitter
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.50f, 0.10f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
			ImGui::Button("##vsplitter1", ImVec2(2.0f, -1));
			ImGui::PopStyleColor(3);

			if (ImGui::IsItemActive()) {
				leftPanelWidth += ImGui::GetIO().MouseDelta.x;
				leftPanelWidth = std::clamp(leftPanelWidth, 150.0f, 400.0f);
			}
			ImGui::SameLine();
		}

		// Content panel
		float contentWidth = ImGui::GetContentRegionAvail().x - (m_ShowPreview ? rightPanelWidth + 2.0f : 0);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		ImGui::BeginChild("ContentPanel", ImVec2(contentWidth, 0), true);
		ImGui::PopStyleColor();

		// Search and breadcrumbs area
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		RenderSearchBar();
		RenderBreadcrumbs();
		ImGui::Separator();
		ImGui::PopStyleVar();

		RenderContentArea();
		ImGui::EndChild();

		// Right panel (Preview)
		if (m_ShowPreview) {
			ImGui::SameLine();

			// Vertical splitter
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.50f, 0.10f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
			ImGui::Button("##vsplitter2", ImVec2(2.0f, -1));
			ImGui::PopStyleColor(3);

			if (ImGui::IsItemActive()) {
				rightPanelWidth += ImGui::GetIO().MouseDelta.x;
				rightPanelWidth = std::clamp(rightPanelWidth, 200.0f, 500.0f);
			}
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
			ImGui::BeginChild("PreviewPanel", ImVec2(0, 0), true);
			ImGui::PopStyleColor();
			RenderFilePreview();
			ImGui::EndChild();
		}

		ImGui::PopStyleVar(); // ItemSpacing

		RenderContextMenu();

		ImGui::End();
	}

	void ContentBrowserPanel::RenderTopBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

		// Navigation buttons styled like UE5
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

		// Back button
		ImGui::BeginDisabled(m_HistoryIndex <= 0);
		if (ImGui::Button("<", ImVec2(30, 30))) {
			NavigateBack();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

		ImGui::SameLine();

		// Forward button
		ImGui::BeginDisabled(m_HistoryIndex >= (int)m_NavigationHistory.size() - 1);
		if (ImGui::Button(">", ImVec2(30, 30))) {
			NavigateForward();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");

		ImGui::SameLine();

		// Up button
		if (ImGui::Button("^", ImVec2(30, 30))) {
			if (m_CurrentDirectory != g_AssetPath) {
				NavigateToDirectory(m_CurrentDirectory.parent_path());
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Up");

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(8, 0));
		ImGui::SameLine();

		// Path display (non-editable for now)
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 250);
		std::string currentPath = m_CurrentDirectory.string();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
		ImGui::InputText("##Path", (char*)currentPath.c_str(), currentPath.length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor();

		ImGui::SameLine();

		// View mode buttons
		ImGui::PushStyleColor(ImGuiCol_Button, m_Settings.CurrentViewMode == ViewMode::Grid ?
			ImVec4(0.90f, 0.50f, 0.10f, 0.6f) : ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::Button("Grid", ImVec2(50, 30))) {
			m_Settings.CurrentViewMode = ViewMode::Grid;
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid View");

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, m_Settings.CurrentViewMode == ViewMode::List ?
			ImVec4(0.90f, 0.50f, 0.10f, 0.6f) : ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::Button("List", ImVec2(50, 30))) {
			m_Settings.CurrentViewMode = ViewMode::List;
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("List View");

		ImGui::SameLine();

		// Settings button
		if (ImGui::Button("...", ImVec2(30, 30))) {
			ImGui::OpenPopup("ContentBrowserSettings");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Settings");

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);

		// Settings popup
		if (ImGui::BeginPopup("ContentBrowserSettings")) {
			ImGui::Text("Content Browser Settings");
			ImGui::Separator();

			ImGui::Checkbox("Show Favorites Panel", &m_ShowFavorites);
			ImGui::Checkbox("Show Preview Panel", &m_ShowPreview);
			ImGui::Separator();

			if (m_Settings.CurrentViewMode == ViewMode::Grid) {
				ImGui::Text("Grid View Settings");
				ImGui::SliderFloat("Thumbnail Size", &m_Settings.ThumbnailSize, 64, 256, "%.0f");
				ImGui::SliderFloat("Padding", &m_Settings.Padding, 8, 32, "%.0f");
			}

			ImGui::Separator();
			ImGui::Text("Sort By:");
			if (ImGui::RadioButton("Name", m_Settings.CurrentSortMode == SortMode::Name))
				m_Settings.CurrentSortMode = SortMode::Name;
			if (ImGui::RadioButton("Type", m_Settings.CurrentSortMode == SortMode::Type))
				m_Settings.CurrentSortMode = SortMode::Type;
			if (ImGui::RadioButton("Size", m_Settings.CurrentSortMode == SortMode::Size))
				m_Settings.CurrentSortMode = SortMode::Size;
			if (ImGui::RadioButton("Date Modified", m_Settings.CurrentSortMode == SortMode::DateModified))
				m_Settings.CurrentSortMode = SortMode::DateModified;

			ImGui::Checkbox("Ascending", &m_Settings.SortAscending);

			ImGui::EndPopup();
		}
	}

	void ContentBrowserPanel::RenderSearchBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer))) {
			m_SearchQuery = m_SearchBuffer;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void ContentBrowserPanel::RenderBreadcrumbs() {
		auto relativePath = std::filesystem::relative(m_CurrentDirectory, g_AssetPath);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.50f, 0.10f, 0.6f));

		if (ImGui::Button("Assets")) {
			NavigateToDirectory(g_AssetPath);
		}

		std::filesystem::path accumulatedPath = g_AssetPath;
		for (auto& part : relativePath) {
			if (part == ".") continue;

			ImGui::SameLine();
			ImGui::TextDisabled(">");
			ImGui::SameLine();

			accumulatedPath /= part;
			if (ImGui::Button(part.string().c_str())) {
				NavigateToDirectory(accumulatedPath);
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void ContentBrowserPanel::RenderFavoritesPanel() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

		ImGui::TextDisabled("FAVORITES");
		ImGui::Separator();
		ImGui::Spacing();

		for (auto it = m_Favorites.begin(); it != m_Favorites.end(); ) {
			bool removeItem = false;
			std::filesystem::path& favPath = *it;
			std::string name = favPath.filename().string();

			ImGui::PushID(favPath.string().c_str());

			bool isSelected = (favPath == m_CurrentDirectory);

			ImGui::PushStyleColor(ImGuiCol_Header, isSelected ?
				ImVec4(0.90f, 0.50f, 0.10f, 0.4f) : ImVec4(0.10f, 0.10f, 0.10f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.90f, 0.50f, 0.10f, 0.6f));

			if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(0, 22))) {
				if (std::filesystem::exists(favPath)) {
					NavigateToDirectory(favPath);
				}
			}

			ImGui::PopStyleColor(3);

			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Remove from Favorites")) {
					removeItem = true;
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();

			if (removeItem) {
				it = m_Favorites.erase(it);
			}
			else {
				++it;
			}
		}

		ImGui::PopStyleVar();
	}

	void ContentBrowserPanel::RenderContentArea() {
		if (m_Settings.CurrentViewMode == ViewMode::Grid) {
			RenderGridView();
		}
		else {
			RenderListView();
		}
	}

	void ContentBrowserPanel::RenderGridView() {
		float padding = m_Settings.Padding;
		float thumbnailSize = m_Settings.ThumbnailSize;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;

		auto entries = GetFilteredAndSortedEntries();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(padding / 2, padding));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

		float startX = ImGui::GetCursorPosX();
		float startY = ImGui::GetCursorPosY();
		int column = 0;

		for (auto& entry : entries) {
			const auto& path = entry.path();
			auto relativePath = std::filesystem::relative(path, g_AssetPath);
			std::string filenameString = path.filename().string();

			ImGui::PushID(filenameString.c_str());

			bool isSelected = (path == m_SelectedItem);
			Ref<Texture2D> icon = entry.is_directory() ? m_DirectoryIcon : m_FileIcon;

			ImGui::BeginGroup();

			// Icon button with UE5 style
			ImVec4 bgColor = isSelected ? ImVec4(0.90f, 0.50f, 0.10f, 0.3f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			ImVec4 hoverColor = isSelected ? ImVec4(0.90f, 0.50f, 0.10f, 0.5f) : ImVec4(0.12f, 0.12f, 0.12f, 0.8f);

			ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.50f, 0.10f, 0.7f));

			if (icon) {
				if (ImGui::ImageButton(filenameString.c_str(),
					(void*)(intptr_t)icon->GetRendererID(),
					{ thumbnailSize, thumbnailSize },
					{ 0, 1 },
					{ 1, 0 }))
				{
					m_SelectedItem = path;
				}

				// Drag and drop
				if (ImGui::BeginDragDropSource()) {
					const wchar_t* itemPath = relativePath.c_str();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
					ImGui::Text("%s", filenameString.c_str());
					ImGui::EndDragDropSource();
				}

				// Double-click to navigate
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					if (entry.is_directory()) {
						NavigateToDirectory(path);
					}
				}

				// Right-click menu
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
					m_RightClickedItem = path;
					m_ShowContextMenu = true;
				}
			}

			ImGui::PopStyleColor(3);

			// Filename label
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + thumbnailSize);
			ImGui::PushStyleColor(ImGuiCol_Text, isSelected ?
				ImVec4(0.92f, 0.92f, 0.92f, 1.0f) : ImVec4(0.75f, 0.75f, 0.75f, 1.0f));

			// Truncate long names
			std::string displayName = filenameString;
			if (displayName.length() > 20) {
				displayName = displayName.substr(0, 17) + "...";
			}

			ImGui::TextWrapped("%s", displayName.c_str());
			ImGui::PopStyleColor();
			ImGui::PopTextWrapPos();

			ImGui::EndGroup();
			ImGui::PopID();

			// Layout management
			column++;
			if (column < columnCount) {
				ImGui::SameLine();
			}
			else {
				column = 0;
			}
		}

		ImGui::PopStyleVar(2);
	}

	void ContentBrowserPanel::RenderListView() {
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 4));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

		if (ImGui::BeginTable("FileList", 4,
			ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			auto entries = GetFilteredAndSortedEntries();

			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.90f, 0.50f, 0.10f, 0.4f));

			for (auto& entry : entries) {
				const auto& path = entry.path();
				std::string filename = path.filename().string();
				bool isSelected = (path == m_SelectedItem);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(filename.c_str());

				ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
				if (ImGui::Selectable(filename.c_str(), isSelected, flags)) {
					m_SelectedItem = path;
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					if (entry.is_directory()) {
						NavigateToDirectory(path);
					}
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
					m_RightClickedItem = path;
					m_ShowContextMenu = true;
				}

				// Type
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", entry.is_directory() ? "Folder" : GetFileExtension(path).c_str());

				// Size
				ImGui::TableNextColumn();
				if (!entry.is_directory()) {
					auto fileSize = std::filesystem::file_size(path);
					ImGui::TextDisabled("%s", GetFileSize(fileSize).c_str());
				}

				// Modified date
				ImGui::TableNextColumn();
				ImGui::TextDisabled("--");

				ImGui::PopID();
			}

			ImGui::PopStyleColor(2);
			ImGui::EndTable();
		}

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

	void ContentBrowserPanel::RenderContextMenu() {
		if (m_ShowContextMenu) {
			ImGui::OpenPopup("ContentBrowserContextMenu");
			m_ShowContextMenu = false;
		}

		if (ImGui::BeginPopup("ContentBrowserContextMenu")) {
			std::string itemName = m_RightClickedItem.filename().string();
			ImGui::TextDisabled("%s", itemName.c_str());
			ImGui::Separator();

			if (ImGui::MenuItem("Open")) {
				if (std::filesystem::is_directory(m_RightClickedItem)) {
					NavigateToDirectory(m_RightClickedItem);
				}
			}

			if (ImGui::MenuItem("Copy Path")) {
				// TODO: Copy to clipboard
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Rename")) {
				// TODO: Implement rename
			}

			if (ImGui::MenuItem("Delete")) {
				// TODO: Implement delete with confirmation
			}

			ImGui::Separator();

			bool isFav = IsFavorite(m_RightClickedItem);
			if (ImGui::MenuItem(isFav ? "Remove from Favorites" : "Add to Favorites")) {
				if (isFav)
					RemoveFromFavorites(m_RightClickedItem);
				else
					AddToFavorites(m_RightClickedItem);
			}

			ImGui::EndPopup();
		}
	}

	void ContentBrowserPanel::RenderFilePreview() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

		ImGui::TextDisabled("PREVIEW");
		ImGui::Separator();
		ImGui::Spacing();

		if (m_SelectedItem.empty() || !std::filesystem::exists(m_SelectedItem)) {
			ImGui::TextDisabled("No file selected");
			ImGui::PopStyleVar();
			return;
		}

		std::string filename = m_SelectedItem.filename().string();

		ImGui::Text("Name:");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
		ImGui::TextWrapped("%s", filename.c_str());
		ImGui::PopStyleColor();

		ImGui::Spacing();
		ImGui::Text("Type:");
		ImGui::SameLine();
		if (std::filesystem::is_directory(m_SelectedItem)) {
			ImGui::TextDisabled("Folder");
		}
		else {
			ImGui::TextDisabled("%s", GetFileExtension(m_SelectedItem).c_str());
		}

		if (!std::filesystem::is_directory(m_SelectedItem)) {
			ImGui::Spacing();
			ImGui::Text("Size:");
			ImGui::SameLine();
			auto size = std::filesystem::file_size(m_SelectedItem);
			ImGui::TextDisabled("%s", GetFileSize(size).c_str());
		}

		ImGui::Spacing();
		ImGui::Text("Path:");
		ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
		ImGui::TextDisabled("%s", m_SelectedItem.string().c_str());
		ImGui::PopTextWrapPos();

		ImGui::PopStyleVar();
	}

	// Navigation methods implementation (unchanged)
	void ContentBrowserPanel::NavigateToDirectory(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
			return;

		m_CurrentDirectory = path;

		if (m_HistoryIndex < (int)m_NavigationHistory.size() - 1) {
			m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
		}

		m_NavigationHistory.push_back(path);
		m_HistoryIndex = (int)m_NavigationHistory.size() - 1;
		m_SelectedItem.clear();
	}

	void ContentBrowserPanel::NavigateBack() {
		if (m_HistoryIndex > 0) {
			m_HistoryIndex--;
			m_CurrentDirectory = m_NavigationHistory[m_HistoryIndex];
			m_SelectedItem.clear();
		}
	}

	void ContentBrowserPanel::NavigateForward() {
		if (m_HistoryIndex < (int)m_NavigationHistory.size() - 1) {
			m_HistoryIndex++;
			m_CurrentDirectory = m_NavigationHistory[m_HistoryIndex];
			m_SelectedItem.clear();
		}
	}

	void ContentBrowserPanel::RefreshCurrentDirectory() {
		m_SelectedItem.clear();
	}

	void ContentBrowserPanel::AddToFavorites(const std::filesystem::path& path) {
		if (!IsFavorite(path)) {
			m_Favorites.push_back(path);
		}
	}

	void ContentBrowserPanel::RemoveFromFavorites(const std::filesystem::path& path) {
		m_Favorites.erase(
			std::remove(m_Favorites.begin(), m_Favorites.end(), path),
			m_Favorites.end()
		);
	}

	bool ContentBrowserPanel::IsFavorite(const std::filesystem::path& path) const {
		return std::find(m_Favorites.begin(), m_Favorites.end(), path) != m_Favorites.end();
	}

	std::vector<std::filesystem::directory_entry> ContentBrowserPanel::GetFilteredAndSortedEntries() {
		std::vector<std::filesystem::directory_entry> entries;

		try {
			for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
				if (PassesSearchFilter(entry)) {
					entries.push_back(entry);
				}
			}
		}
		catch (const std::exception& e) {
			// Handle permission errors or invalid directories
			return entries;
		}

		// Sort entries
		std::sort(entries.begin(), entries.end(), [this](const auto& a, const auto& b) {
			// Directories first
			bool aIsDir = a.is_directory();
			bool bIsDir = b.is_directory();
			if (aIsDir != bIsDir) return aIsDir;

			bool result = false;
			switch (m_Settings.CurrentSortMode) {
			case SortMode::Name:
				result = a.path().filename().string() < b.path().filename().string();
				break;
			case SortMode::Type:
				result = GetFileExtension(a.path()) < GetFileExtension(b.path());
				break;
			case SortMode::Size:
				if (!aIsDir && !bIsDir)
					result = std::filesystem::file_size(a.path()) < std::filesystem::file_size(b.path());
				break;
			case SortMode::DateModified:
				result = std::filesystem::last_write_time(a.path()) < std::filesystem::last_write_time(b.path());
				break;
			}

			return m_Settings.SortAscending ? result : !result;
			});

		return entries;
	}

	bool ContentBrowserPanel::PassesSearchFilter(const std::filesystem::directory_entry& entry) {
		if (m_SearchQuery.empty()) return true;

		std::string filename = entry.path().filename().string();
		std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

		std::string query = m_SearchQuery;
		std::transform(query.begin(), query.end(), query.begin(), ::tolower);

		return filename.find(query) != std::string::npos;
	}

	std::string ContentBrowserPanel::GetFileSize(uintmax_t size) {
		const char* units[] = { "B", "KB", "MB", "GB", "TB" };
		int unitIndex = 0;
		double displaySize = (double)size;

		while (displaySize >= 1024.0 && unitIndex < 4) {
			displaySize /= 1024.0;
			unitIndex++;
		}

		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%.2f %s", displaySize, units[unitIndex]);
		return std::string(buffer);
	}

	std::string ContentBrowserPanel::GetFileExtension(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		if (!ext.empty() && ext[0] == '.') {
			ext = ext.substr(1);
		}
		std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
		return ext.empty() ? "File" : ext;
	}

	DirectoryInfo ContentBrowserPanel::GetDirectoryInfo(const std::filesystem::path& path) {
		DirectoryInfo info;
		info.Path = path;

		try {
			for (const auto& entry : std::filesystem::directory_iterator(path)) {
				if (entry.is_directory()) {
					info.DirectoryCount++;
				}
				else {
					info.FileCount++;
					info.TotalSize += std::filesystem::file_size(entry.path());
				}
			}
		}
		catch (...) {
			// Handle errors silently
		}

		return info;
	}

}