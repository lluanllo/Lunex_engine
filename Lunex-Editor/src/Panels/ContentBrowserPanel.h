#pragma once

#include "Renderer/Texture.h"
#include <filesystem>
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>
#include <imgui.h>
#include <imgui_internal.h>

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

		// Icons & Thumbnails
		Ref<Texture2D> GetIconForFile(const std::filesystem::path& path);
		Ref<Texture2D> GetThumbnailForFile(const std::filesystem::path& path);

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

		// Selection
		bool IsSelected(const std::filesystem::path& path) const;
		void AddToSelection(const std::filesystem::path& path);
		void RemoveFromSelection(const std::filesystem::path& path);
		void ToggleSelection(const std::filesystem::path& path);
		void SelectRange(const std::filesystem::path& from, const std::filesystem::path& to);

		// Utilities
		std::string GetFileSizeString(uintmax_t size);
		std::string GetLastModifiedString(const std::filesystem::path& path);

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
		char m_NewItemName[256] = "NewFolder";
		std::filesystem::path m_ItemToRename;

		// File drop
		bool m_IsHovered = false;
		std::filesystem::path m_HoveredFolder;

		// Selection
		std::set<std::string> m_SelectedItems; // Store paths as strings (changed to set for consistency)
		std::filesystem::path m_LastSelectedItem;
		bool m_IsSelecting = false;
		ImVec2 m_SelectionStart;
		ImVec2 m_SelectionEnd;

		// View settings
		float m_ThumbnailSize = 80.0f;
		float m_Padding = 12.0f;

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
		Ref<Texture2D> m_MeshIcon;  // Icon for .lumesh files
		Ref<Texture2D> m_PrefabIcon;  // Icon for .luprefab files

		// Texture preview cache
		std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;

		// Item positions for selection rectangle
		std::unordered_map<std::string, ImRect> m_ItemBounds;

		// Clipboard
		enum class ClipboardOperation { None, Copy, Cut };
		ClipboardOperation m_ClipboardOperation = ClipboardOperation::None;
		std::vector<std::filesystem::path> m_ClipboardItems;
		
		// Callbacks
		std::function<void(const std::filesystem::path&)> m_OnMaterialOpenCallback;
	};
}