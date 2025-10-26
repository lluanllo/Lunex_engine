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
		m_Favorites.push_back(g_AssetPath / "Textures");
		m_Favorites.push_back(g_AssetPath / "Models");
		m_Favorites.push_back(g_AssetPath / "Scenes");
	}

	void ContentBrowserPanel::OnImGuiRender() {
		ImGui::Begin("Content Browser");

		RenderTopBar();
		ImGui::Separator();

		// Main layout with splitters
		static float leftPanelWidth = m_FavoritesPanelWidth;
		static float rightPanelWidth = m_PreviewPanelWidth;

		ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0), true);
		if (m_ShowFavorites)
			RenderFavoritesPanel();
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::InvisibleButton("##vsplitter1", ImVec2(4.0f, -1));
		if (ImGui::IsItemActive()) {
			leftPanelWidth += ImGui::GetIO().MouseDelta.x;
			leftPanelWidth = std::clamp(leftPanelWidth, 150.0f, 400.0f);
		}
		ImGui::SameLine();

		float contentWidth = ImGui::GetContentRegionAvail().x - (m_ShowPreview ? rightPanelWidth + 4.0f : 0);
		ImGui::BeginChild("ContentPanel", ImVec2(contentWidth, 0), true);
		RenderSearchBar();
		RenderBreadcrumbs();
		ImGui::Separator();
		RenderContentArea();
		ImGui::EndChild();

		if (m_ShowPreview) {
			ImGui::SameLine();
			ImGui::InvisibleButton("##vsplitter2", ImVec2(4.0f, -1));
			if (ImGui::IsItemActive()) {
				rightPanelWidth += ImGui::GetIO().MouseDelta.x;
				rightPanelWidth = std::clamp(rightPanelWidth, 200.0f, 500.0f);
			}
			ImGui::SameLine();

			ImGui::BeginChild("PreviewPanel", ImVec2(0, 0), true);
			RenderFilePreview();
			ImGui::EndChild();
		}

		RenderContextMenu();
		RenderStatusBar();

		ImGui::End();
	}

	void ContentBrowserPanel::RenderTopBar() {
		// Navigation buttons
		ImGui::BeginDisabled(m_HistoryIndex <= 0);
		if (ImGui::Button("<##back") || (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && ImGui::IsKeyDown(ImGuiKey_LeftAlt))) {
			NavigateBack();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back (Alt+Left)");

		ImGui::SameLine();
		ImGui::BeginDisabled(m_HistoryIndex >= (int)m_NavigationHistory.size() - 1);
		if (ImGui::Button(">##forward") || (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && ImGui::IsKeyDown(ImGuiKey_LeftAlt))) {
			NavigateForward();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward (Alt+Right)");

		ImGui::SameLine();
		if (ImGui::Button("^##up")) {
			if (m_CurrentDirectory != g_AssetPath) {
				NavigateToDirectory(m_CurrentDirectory.parent_path());
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Up (Alt+Up)");

		ImGui::SameLine();
		if (ImGui::Button("R##refresh") || ImGui::IsKeyPressed(ImGuiKey_F5)) {
			RefreshCurrentDirectory();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh (F5)");

		// View mode toggles
		ImGui::SameLine(ImGui::GetWindowWidth() - 180);
		if (ImGui::Button("Grid")) {
			m_Settings.CurrentViewMode = ViewMode::Grid;
		}
		ImGui::SameLine();
		if (ImGui::Button("List")) {
			m_Settings.CurrentViewMode = ViewMode::List;
		}

		// Settings
		ImGui::SameLine();
		if (ImGui::Button("...")) {
			ImGui::OpenPopup("Settings");
		}

		if (ImGui::BeginPopup("Settings")) {
			ImGui::Checkbox("Show Favorites Panel", &m_ShowFavorites);
			ImGui::Checkbox("Show Preview Panel", &m_ShowPreview);
			ImGui::Checkbox("Show Hidden Files", &m_Settings.ShowHiddenFiles);
			ImGui::Separator();

			if (m_Settings.CurrentViewMode == ViewMode::Grid) {
				ImGui::SliderFloat("Thumbnail Size", &m_Settings.ThumbnailSize, 64, 256);
				ImGui::SliderFloat("Padding", &m_Settings.Padding, 8, 32);
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
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer))) {
			m_SearchQuery = m_SearchBuffer;
		}
	}

	void ContentBrowserPanel::RenderBreadcrumbs() {
		auto relativePath = std::filesystem::relative(m_CurrentDirectory, g_AssetPath);

		if (ImGui::Button("Assets")) {
			NavigateToDirectory(g_AssetPath);
		}

		std::filesystem::path accumulatedPath = g_AssetPath;
		for (auto& part : relativePath) {
			if (part == ".") continue;

			ImGui::SameLine();
			ImGui::Text(">");
			ImGui::SameLine();

			accumulatedPath /= part;
			if (ImGui::Button(part.string().c_str())) {
				NavigateToDirectory(accumulatedPath);
			}
		}

		// Add to favorites button
		ImGui::SameLine(ImGui::GetWindowWidth() - 80);
		bool isFav = IsFavorite(m_CurrentDirectory);
		if (isFav) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
		if (ImGui::Button(isFav ? "* Favorited" : "+ Favorite")) {
			if (isFav)
				RemoveFromFavorites(m_CurrentDirectory);
			else
				AddToFavorites(m_CurrentDirectory);
		}
		if (isFav) ImGui::PopStyleColor();
	}

	void ContentBrowserPanel::RenderFavoritesPanel() {
		ImGui::Text("Favorites");
		ImGui::Separator();

		for (auto it = m_Favorites.begin(); it != m_Favorites.end(); ) {
			bool removeItem = false;
			std::filesystem::path& favPath = *it;
			std::string name = favPath.filename().string();

			ImGui::PushID(favPath.string().c_str());

			// Selectable favorite
			bool isSelected = (favPath == m_CurrentDirectory);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
				if (std::filesystem::exists(favPath)) {
					NavigateToDirectory(favPath);
				}
			}

			// Right-click menu
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

		ImGui::Columns(columnCount, 0, false);

		auto entries = GetFilteredAndSortedEntries();

		for (auto& entry : entries) {
			const auto& path = entry.path();
			auto relativePath = std::filesystem::relative(path, g_AssetPath);
			std::string filenameString = path.filename().string();

			ImGui::PushID(filenameString.c_str());
			Ref<Texture2D> icon = entry.is_directory() ? m_DirectoryIcon : m_FileIcon;

			bool isSelected = (path == m_SelectedItem);
			ImVec4 bgColor = isSelected ? ImVec4(0.3f, 0.5f, 0.8f, 0.4f) : ImVec4(0, 0, 0, 0);

			ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.3f, 0.5f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.5f, 0.8f, 0.6f));

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
					m_ContextMenuPos = ImGui::GetMousePos();
				}
			}

			ImGui::PopStyleColor(3);

			// Filename with wrapping
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + thumbnailSize);
			ImGui::TextWrapped("%s", filenameString.c_str());
			ImGui::PopTextWrapPos();

			ImGui::PopID();
			ImGui::NextColumn();
		}

		ImGui::Columns(1);
	}

	void ContentBrowserPanel::RenderListView() {
		if (ImGui::BeginTable("FileList", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableHeadersRow();

			auto entries = GetFilteredAndSortedEntries();

			for (auto& entry : entries) {
				const auto& path = entry.path();
				std::string filename = path.filename().string();
				bool isSelected = (path == m_SelectedItem);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(filename.c_str());

				if (ImGui::Selectable(filename.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
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
				ImGui::Text("%s", entry.is_directory() ? "Folder" : GetFileExtension(path).c_str());

				// Size
				ImGui::TableNextColumn();
				if (!entry.is_directory()) {
					auto fileSize = std::filesystem::file_size(path);
					ImGui::Text("%s", GetFileSize(fileSize).c_str());
				}

				// Modified date
				ImGui::TableNextColumn();
				auto ftime = std::filesystem::last_write_time(path);
				// Note: Converting file_time to system time is complex, showing placeholder
				ImGui::Text("--");

				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}

	void ContentBrowserPanel::RenderContextMenu() {
		if (m_ShowContextMenu) {
			ImGui::OpenPopup("ContextMenu");
			m_ShowContextMenu = false;
		}

		if (ImGui::BeginPopup("ContextMenu")) {
			std::string itemName = m_RightClickedItem.filename().string();
			ImGui::Text("%s", itemName.c_str());
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

	void ContentBrowserPanel::RenderStatusBar() {
		ImGui::Separator();
		auto dirInfo = GetDirectoryInfo(m_CurrentDirectory);
		ImGui::Text("%zu items (%zu folders, %zu files) | %s",
			dirInfo.FileCount + dirInfo.DirectoryCount,
			dirInfo.DirectoryCount,
			dirInfo.FileCount,
			GetFileSize(dirInfo.TotalSize).c_str());

		if (!m_SelectedItem.empty() && std::filesystem::exists(m_SelectedItem)) {
			ImGui::SameLine();
			ImGui::Text(" | Selected: %s", m_SelectedItem.filename().string().c_str());
		}
	}

	void ContentBrowserPanel::RenderFilePreview() {
		ImGui::Text("Preview");
		ImGui::Separator();

		if (m_SelectedItem.empty() || !std::filesystem::exists(m_SelectedItem)) {
			ImGui::TextWrapped("No file selected");
			return;
		}

		std::string filename = m_SelectedItem.filename().string();
		ImGui::Text("Name:");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", filename.c_str());

		ImGui::Spacing();
		ImGui::Text("Type:");
		ImGui::SameLine();
		if (std::filesystem::is_directory(m_SelectedItem)) {
			ImGui::Text("Folder");
		}
		else {
			ImGui::Text("%s", GetFileExtension(m_SelectedItem).c_str());
		}

		if (!std::filesystem::is_directory(m_SelectedItem)) {
			ImGui::Spacing();
			ImGui::Text("Size:");
			ImGui::SameLine();
			auto size = std::filesystem::file_size(m_SelectedItem);
			ImGui::Text("%s", GetFileSize(size).c_str());
		}

		ImGui::Spacing();
		ImGui::Text("Path:");
		ImGui::TextWrapped("%s", m_SelectedItem.string().c_str());

		// TODO: Add thumbnail preview for images
		// TODO: Add metadata for different file types
	}

	void ContentBrowserPanel::NavigateToDirectory(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
			return;

		m_CurrentDirectory = path;

		// Update navigation history
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
		// Force refresh by clearing any cached data
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

		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			if (!m_Settings.ShowHiddenFiles) {
				if (entry.path().filename().string()[0] == '.') {
					continue;
				}
			}

			if (PassesSearchFilter(entry)) {
				entries.push_back(entry);
			}
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
		return ext.empty() ? "File" : ext + " File";
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