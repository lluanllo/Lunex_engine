#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "ContentBrowser/ThumbnailManager.h"
#include "ContentBrowser/FileOperations.h"
#include "ContentBrowser/ContentBrowserNavigation.h"
#include "ContentBrowser/ContentBrowserSelection.h"
#include "ContentBrowser/AssetCardRenderer.h"
#include <filesystem>
#include <functional>

namespace Lunex {
	class Event;

	// External declaration for asset path
	extern const std::filesystem::path g_AssetPath;

	// ============================================================================
	// CONTENT BROWSER PAYLOAD - For drag & drop operations
	// ============================================================================
	struct ContentBrowserPayload {
		char FilePath[512];      // Full file path
		char RelativePath[512];  // Path relative to Assets
		char Extension[32];      // File extension (lowercase with dot, e.g., ".png")
		bool IsDirectory;        // true if folder
		int ItemCount;           // Always 1 for single item
	};

	// ============================================================================
	// CONTENT BROWSER PANEL - AAA Editor Architecture
	// ============================================================================
	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel() = default;

		void OnImGuiRender();
		void OnEvent(Event& e);
		void SetRootDirectory(const std::filesystem::path& directory);

		const std::filesystem::path& GetCurrentDirectory() const { 
			return m_Navigation.GetCurrentDirectory(); 
		}

		// ========================================
		// CALLBACKS
		// ========================================
		void SetOnMaterialOpenCallback(const std::function<void(const std::filesystem::path&)>& callback) {
			m_OnMaterialOpenCallback = callback;
		}

		// ========================================
		// CACHE MANAGEMENT
		// ========================================
		void InvalidateMaterialThumbnail(const std::filesystem::path& materialPath);
		void InvalidateThumbnailDiskCache(const std::filesystem::path& materialPath);
		void RefreshAllThumbnails();

		// ========================================
		// PUBLIC API FOR GLOBAL SHORTCUTS
		// ========================================
		void SelectAll();
		void ClearSelection() { m_Selection.Clear(); }
		void DeleteSelectedItems();
		void DuplicateSelectedItem();
		void RenameSelectedItem();
		void CopySelectedItems() { m_Selection.CopySelection(); }
		void CutSelectedItems() { m_Selection.CutSelection(); }
		void PasteItems();
		void NavigateBack() { m_Navigation.NavigateBack(); }
		void NavigateForward() { m_Navigation.NavigateForward(); }
		void NavigateUp() { m_Navigation.NavigateToParent(); }

		// Selection queries
		bool HasSelection() const { return m_Selection.HasSelection(); }

	private:
		// Rendering
		void RenderTopBar();
		void RenderSidebar();
		void RenderFileGrid();
		void RenderContextMenu();
		void RenderDirectoryTree(const std::filesystem::path& path);

		// Dialogs
		void RenderCreateFolderDialog();
		void RenderRenameDialog();

		// Event handling
		void HandleFileDrop(const std::vector<std::string>& files);
		void HandleItemDoubleClick(const std::filesystem::path& path, bool isDirectory);
		void HandleItemClick(const std::filesystem::path& path, bool ctrlDown, bool shiftDown);
		void HandleItemRightClick(const std::filesystem::path& path);

		// Drag & drop
		void SetupDragDropSource(const std::filesystem::path& path, bool isDirectory);
		void SetupDragDropTarget(const std::filesystem::path& targetFolder);

	private:
		// Subsystems
		ThumbnailManager m_ThumbnailManager;
		ContentBrowserFileOperations m_FileOperations;
		ContentBrowserNavigation m_Navigation;
		ContentBrowserSelection m_Selection;
		AssetCardRenderer m_CardRenderer;

		// Search
		char m_SearchBuffer[256] = "";

		// Context menu state
		bool m_ShowCreateFolderDialog = false;
		bool m_ShowRenameDialog = false;
		char m_NewItemName[256] = "NewFolder";
		std::filesystem::path m_ItemToRename;

		// File drop tracking
		bool m_IsHovered = false;
		std::filesystem::path m_HoveredFolder;

		// Callbacks
		std::function<void(const std::filesystem::path&)> m_OnMaterialOpenCallback;
	};

} // namespace Lunex
