#pragma once

#include "../UI/LunexUI.h"
#include "Renderer/Texture.h"
#include "Renderer/MaterialPreviewRenderer.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Assets/Mesh/MeshAsset.h"
#include <filesystem>
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>

namespace Lunex {
	class Event;

	// Declaración externa para que otros archivos puedan usar g_AssetPath
	extern const std::filesystem::path g_AssetPath;

	// ============================================================================
	// ESTRUCTURA DE PAYLOAD PARA DRAG & DROP
	// ============================================================================
	struct ContentBrowserPayload {
		char FilePath[512];      // Path completo del archivo
		char RelativePath[512];  // Path relativo desde Assets
		char Extension[32];      // Extensión del archivo (lowercase con punto, ej: ".png")
		bool IsDirectory;        // true si es carpeta
		int ItemCount;           // Siempre 1 para single item
	};

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();

		void OnImGuiRender();
		void OnEvent(Event& e);
		void SetRootDirectory(const std::filesystem::path& directory);

		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		
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
		void RefreshAllThumbnails();
		
		// NEW: Invalidate disk cache for thumbnails
		void InvalidateThumbnailDiskCache(const std::filesystem::path& materialPath);

		// ========================================
		// PUBLIC API FOR GLOBAL SHORTCUTS
		// ========================================
		void SelectAll();
		void ClearSelection();
		void DeleteSelectedItems();
		void DuplicateSelectedItem();
		void RenameSelectedItem();
		void CopySelectedItems();
		void CutSelectedItems();
		void PasteItems();
		void NavigateBack();
		void NavigateForward();
		void NavigateUp();
		
		// Selection queries
		bool HasSelection() const { return !m_SelectedItems.empty(); }

	private:
		// Rendering
		void RenderTopBar();
		void RenderSidebar();
		void RenderFileGrid();
		void RenderContextMenu();
		void RenderBottomBar();
		
		// Item context menus
		void RenderItemContextMenu(const std::filesystem::path& path);

		// Icons & Thumbnails
		Ref<Texture2D> GetIconForFile(const std::filesystem::path& path);
		Ref<Texture2D> GetThumbnailForFile(const std::filesystem::path& path);
		std::string GetAssetTypeLabel(const std::filesystem::path& path);
		
		// Build items for FileGrid
		std::vector<UI::FileGridItem> BuildFileGridItems();

		// File operations
		void CreateNewFolder();
		void CreateNewScene();
		void CreateNewScript();
		void CreateNewMaterial();
		void CreatePrefabFromMesh(const std::filesystem::path& meshAssetPath);
		void DeleteItem(const std::filesystem::path& path);
		void RenameItem(const std::filesystem::path& oldPath);
		void DuplicateItem(const std::filesystem::path& path);
		void ImportFiles(const std::vector<std::string>& files);
		void ImportFilesToFolder(const std::vector<std::string>& files, const std::filesystem::path& targetFolder);

		// Clipboard operations
		bool CanPaste() const;

		// Navigation
		void NavigateToParent();
		void NavigateToDirectory(const std::filesystem::path& directory);
		void AddToHistory(const std::filesystem::path& directory);

		// Selection
		bool IsSelected(const std::filesystem::path& path) const;
		void AddToSelection(const std::filesystem::path& path);
		void RemoveFromSelection(const std::filesystem::path& path);
		void ToggleSelection(const std::filesystem::path& path);
		void SelectRange(const std::filesystem::path& from, const std::filesystem::path& to);
		
		// File Grid callbacks
		void OnFileGridItemClicked(const std::filesystem::path& path);
		void OnFileGridItemDoubleClicked(const std::filesystem::path& path);
		void OnFileGridItemRightClicked(const std::filesystem::path& path);
		void OnFileGridItemDropped(const std::filesystem::path& target, const void* data, size_t size);
		void OnFileGridMultiItemDropped(const std::filesystem::path& target, const std::string& data);
		
		// Directory tree callbacks
		void OnDirectorySelected(const std::filesystem::path& path);
		void OnDirectorySingleItemDropped(const std::filesystem::path& target, const void* data, size_t size);
		void OnDirectoryMultiItemDropped(const std::filesystem::path& target, const std::string& data);

		// Utilities
		std::string GetFileSizeString(uintmax_t size);
		std::string GetLastModifiedString(const std::filesystem::path& path);
		std::string GetDisplayName(const std::filesystem::path& path);
		bool MatchesSearchFilter(const std::string& filename);

	private:
		std::filesystem::path m_BaseDirectory;
		std::filesystem::path m_CurrentDirectory;

		// History navigation
		std::vector<std::filesystem::path> m_DirectoryHistory;
		int m_HistoryIndex = 0;

		// Search
		char m_SearchBuffer[256] = "";

		// Context menu
		bool m_ShowCreateFolderDialog = false;
		bool m_ShowRenameDialog = false;
		bool m_OpenItemContextMenu = false;
		bool m_OpenEmptySpaceContextMenu = false;
		char m_NewItemName[256] = "NewFolder";
		std::filesystem::path m_ItemToRename;
		std::filesystem::path m_ContextMenuTargetItem;

		// File drop
		bool m_IsHovered = false;
		bool m_WasFocused = false;
		std::filesystem::path m_HoveredFolder;

		// Selection
		std::set<std::string> m_SelectedItems;
		std::filesystem::path m_LastSelectedItem;

		// View settings
		float m_ThumbnailSize = 96.0f;
		float m_SidebarWidth = 200.0f;
		static constexpr float MIN_SIDEBAR_WIDTH = 150.0f;
		static constexpr float MAX_SIDEBAR_WIDTH = 400.0f;

		// Icons
		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
		Ref<Texture2D> m_BackIcon;
		Ref<Texture2D> m_ForwardIcon;

		// File type icons
		Ref<Texture2D> m_SceneIcon;
		Ref<Texture2D> m_TextureIcon;
		Ref<Texture2D> m_ShaderIcon;
		Ref<Texture2D> m_AudioIcon;
		Ref<Texture2D> m_ScriptIcon;
		Ref<Texture2D> m_MaterialIcon;
		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_PrefabIcon;

		// Texture preview cache
		std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;
		
		// Material preview renderer and cache
		Scope<MaterialPreviewRenderer> m_MaterialPreviewRenderer;
		std::unordered_map<std::string, Ref<Texture2D>> m_MaterialThumbnailCache;
		
		// Mesh/Prefab preview cache
		std::unordered_map<std::string, Ref<Texture2D>> m_MeshThumbnailCache;

		// Clipboard
		enum class ClipboardOperation { None, Copy, Cut };
		ClipboardOperation m_ClipboardOperation = ClipboardOperation::None;
		std::vector<std::filesystem::path> m_ClipboardItems;
		
		// Callbacks
		std::function<void(const std::filesystem::path&)> m_OnMaterialOpenCallback;
		
		// UI Components
		UI::FileGrid m_FileGrid;
		UI::DirectoryTree m_DirectoryTree;
		UI::NavigationBar m_NavigationBar;
		UI::Splitter m_Splitter;
	};
}
