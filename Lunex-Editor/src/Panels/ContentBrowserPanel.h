#pragma once

#include "Renderer/Texture.h"

#include <filesystem>
#include <vector>
#include <string>
#include <imgui.h>

namespace Lunex {

	// Declaración externa para que otros archivos puedan usar g_AssetPath
	extern const std::filesystem::path g_AssetPath;

	enum class ViewMode {
		Grid,
		List
	};

	enum class SortMode {
		Name,
		Type,
		Size,
		DateModified
	};

	struct ContentBrowserSettings {
		float ThumbnailSize = 128.0f;
		float Padding = 16.0f;
		ViewMode CurrentViewMode = ViewMode::Grid;
		SortMode CurrentSortMode = SortMode::Name;
		bool SortAscending = true;
		bool ShowHiddenFiles = false;
	};

	struct DirectoryInfo {
		std::filesystem::path Path;
		size_t FileCount = 0;
		size_t DirectoryCount = 0;
		uintmax_t TotalSize = 0;
	};

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();

		void OnImGuiRender();

	private:
		// Rendering methods
		void RenderTopBar();
		void RenderSearchBar();
		void RenderBreadcrumbs();
		void RenderFavoritesPanel();
		void RenderContentArea();
		void RenderGridView();
		void RenderListView();
		void RenderContextMenu();
		void RenderStatusBar();
		void RenderFilePreview();

		// Navigation methods
		void NavigateToDirectory(const std::filesystem::path& path);
		void NavigateBack();
		void NavigateForward();
		void RefreshCurrentDirectory();

		// Favorites methods
		void AddToFavorites(const std::filesystem::path& path);
		void RemoveFromFavorites(const std::filesystem::path& path);
		bool IsFavorite(const std::filesystem::path& path) const;

		// Utility methods
		std::vector<std::filesystem::directory_entry> GetFilteredAndSortedEntries();
		bool PassesSearchFilter(const std::filesystem::directory_entry& entry);
		std::string GetFileSize(uintmax_t size);
		std::string GetFileExtension(const std::filesystem::path& path);
		DirectoryInfo GetDirectoryInfo(const std::filesystem::path& path);

	private:
		// Current state
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedItem;
		std::filesystem::path m_RightClickedItem;

		// Navigation history
		std::vector<std::filesystem::path> m_NavigationHistory;
		int m_HistoryIndex = -1;

		// Favorites
		std::vector<std::filesystem::path> m_Favorites;

		// Icons
		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
		Ref<Texture2D> m_BackIcon;
		Ref<Texture2D> m_ForwardIcon;
		Ref<Texture2D> m_RefreshIcon;
		Ref<Texture2D> m_FavoriteIcon;

		// Settings
		ContentBrowserSettings m_Settings;

		// Search
		char m_SearchBuffer[256] = "";
		std::string m_SearchQuery;

		// UI State
		bool m_ShowFavorites = true;
		bool m_ShowPreview = true;
		float m_FavoritesPanelWidth = 200.0f;
		float m_PreviewPanelWidth = 300.0f;

		// Context menu state
		bool m_ShowContextMenu = false;
		ImVec2 m_ContextMenuPos;
	};

}