/**
 * @file DirectoryTree.h
 * @brief Directory Tree Component for Content Browser Sidebar
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <filesystem>
#include <functional>

namespace Lunex::UI {

	// ============================================================================
	// DIRECTORY TREE COMPONENT
	// ============================================================================
	
	struct DirectoryTreeStyle {
		float indentSpacing = 12.0f;
		float itemSpacing = 2.0f;
		float iconSize = 16.0f;
		float arrowWidth = 20.0f;
		Color headerColor = Color(0.14f, 0.14f, 0.14f, 0.0f);
		Color hoverColor = Color(0.18f, 0.18f, 0.18f, 1.0f);
		Color selectedColor = Color(0.91f, 0.57f, 0.18f, 0.30f);
		Color textColor = Color(0.92f, 0.92f, 0.92f, 1.0f);
		Color labelColor = Color(0.55f, 0.55f, 0.55f, 1.0f);
	};
	
	struct DirectoryTreeCallbacks {
		std::function<void(const std::filesystem::path&)> onDirectorySelected;
		std::function<void(const std::filesystem::path&, const std::string&)> onFilesDropped;  // target, payload
		std::function<void(const std::filesystem::path&, const void*, size_t)> onSingleItemDropped;
	};
	
	/**
	 * @class DirectoryTree
	 * @brief Renders a hierarchical directory tree with drag-drop support
	 * 
	 * Features:
	 * - Recursive directory display
	 * - Folder icons
	 * - Selection highlighting
	 * - Drag & drop targets
	 */
	class DirectoryTree {
	public:
		DirectoryTree() = default;
		~DirectoryTree() = default;
		
		/**
		 * @brief Render the directory tree
		 * @param rootPath Root directory path
		 * @param rootLabel Label for root node (e.g., "Assets")
		 * @param currentDirectory Currently selected directory
		 * @param folderIcon Icon texture for folders
		 * @param callbacks Event callbacks
		 */
		void Render(const std::filesystem::path& rootPath,
					const std::string& rootLabel,
					const std::filesystem::path& currentDirectory,
					Ref<Texture2D> folderIcon,
					const DirectoryTreeCallbacks& callbacks);
		
		// Style configuration
		void SetStyle(const DirectoryTreeStyle& style) { m_Style = style; }
		DirectoryTreeStyle& GetStyle() { return m_Style; }
		const DirectoryTreeStyle& GetStyle() const { return m_Style; }
		
	private:
		DirectoryTreeStyle m_Style;
		
		void RenderDirectoryNode(const std::filesystem::path& path,
								 const std::filesystem::path& currentDirectory,
								 Ref<Texture2D> folderIcon,
								 const DirectoryTreeCallbacks& callbacks);
		
		void RenderFolderIcon(Ref<Texture2D> icon, ImVec2 cursorPos);
		bool HasSubdirectories(const std::filesystem::path& path);
		void HandleDragDropTarget(const std::filesystem::path& targetPath,
								  const DirectoryTreeCallbacks& callbacks);
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	void RenderDirectoryTree(const std::filesystem::path& rootPath,
							 const std::string& rootLabel,
							 const std::filesystem::path& currentDirectory,
							 Ref<Texture2D> folderIcon,
							 const DirectoryTreeCallbacks& callbacks,
							 const DirectoryTreeStyle& style = DirectoryTreeStyle());

} // namespace Lunex::UI
