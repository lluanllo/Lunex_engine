/**
 * @file FileGrid.h
 * @brief File Grid Component for Content Browser
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <filesystem>
#include <functional>
#include <set>
#include <unordered_map>

namespace Lunex::UI {

	// ============================================================================
	// FILE GRID COMPONENT
	// ============================================================================
	
	struct FileGridStyle {
		float thumbnailSize = 96.0f;
		float padding = 12.0f;
		float cardRounding = 4.0f;
		float cardPadding = 8.0f;
		Color cardBgColor = Colors::BgCard();
		Color iconBgColor = Colors::BgDark();
		Color shadowColor = Colors::Shadow();
		Color textColor = Colors::TextPrimary();
		Color typeColor = Colors::TextMuted();
		Color hoverColor = Colors::BgHover();
		Color selectedBorderColor = Colors::SelectedBorder();
		Color selectedFillColor = Colors::Selected();
		Color selectionRectFill = Color(0.05f, 0.65f, 0.77f, 0.12f);
		Color selectionRectBorder = Color(0.05f, 0.65f, 0.77f, 0.55f);
		Color dropTargetColor = Colors::Primary();
	};
	
	struct FileGridItem {
		std::filesystem::path path;
		std::string name;
		std::string typeLabel;
		Ref<Texture2D> thumbnail;
		bool isDirectory = false;
		bool isHDR = false;  // For ultra-wide aspect ratio
	};
	
	struct FileGridResult {
		std::filesystem::path clickedItem;
		std::filesystem::path doubleClickedItem;
		std::filesystem::path rightClickedItem;
		std::filesystem::path hoveredFolder;  // For drag-drop
		bool clickedEmptySpace = false;
		bool isSelecting = false;
	};
	
	struct FileGridCallbacks {
		std::function<void(const std::filesystem::path&)> onItemClicked;
		std::function<void(const std::filesystem::path&)> onItemDoubleClicked;
		std::function<void(const std::filesystem::path&)> onItemRightClicked;
		std::function<void(const std::filesystem::path&, const void*, size_t)> onItemDropped;
		std::function<void(const std::filesystem::path&, const std::string&)> onMultiItemDropped;
		std::function<void()> onEmptySpaceClicked;
		std::function<void()> onEmptySpaceRightClicked;
	};
	
	/**
	 * @class FileGrid
	 * @brief Renders a grid of file/folder cards with thumbnails
	 * 
	 * Features:
	 * - Thumbnail preview
	 * - Selection (single, multi, range, rectangle)
	 * - Drag & drop (source and target)
	 * - HDR ultra-wide cards
	 * - Folder/File differentiation
	 */
	class FileGrid {
	public:
		FileGrid() = default;
		~FileGrid() = default;
		
		/**
		 * @brief Render the file grid
		 * @param items Vector of items to display
		 * @param selectedItems Set of selected item paths
		 * @param callbacks Event callbacks
		 * @return Interaction results
		 */
		FileGridResult Render(const std::vector<FileGridItem>& items,
							  std::set<std::string>& selectedItems,
							  const FileGridCallbacks& callbacks);
		
		// Style configuration
		void SetStyle(const FileGridStyle& style) { m_Style = style; }
		FileGridStyle& GetStyle() { return m_Style; }
		const FileGridStyle& GetStyle() const { return m_Style; }
		
		void SetThumbnailSize(float size) { m_Style.thumbnailSize = size; }
		float GetThumbnailSize() const { return m_Style.thumbnailSize; }
		
		// Selection rectangle state (for external management)
		bool IsSelectingRectangle() const { return m_IsSelecting; }
		void SetSelectingRectangle(bool selecting) { m_IsSelecting = selecting; }
		ImVec2 GetSelectionStart() const { return m_SelectionStart; }
		ImVec2 GetSelectionEnd() const { return m_SelectionEnd; }
		
	private:
		FileGridStyle m_Style;
		
		// Selection rectangle state
		bool m_IsSelecting = false;
		ImVec2 m_SelectionStart;
		ImVec2 m_SelectionEnd;
		
		// Item bounds for selection rectangle
		std::unordered_map<std::string, ImRect> m_ItemBounds;
		
		void RenderFolderCard(const FileGridItem& item, ImVec2 cursorPos, ImDrawList* drawList);
		void RenderFileCard(const FileGridItem& item, ImVec2 cursorPos, ImDrawList* drawList);
		void RenderSelectionEffects(ImVec2 cardMin, ImVec2 cardMax, bool isSelected, bool isHovered, bool isDropTarget);
		void RenderSelectionRectangle();
		
		float GetCardWidth(const FileGridItem& item) const;
		float GetCardHeight(const FileGridItem& item) const;
		
		void HandleDragDropSource(const FileGridItem& item, const std::set<std::string>& selectedItems);
		void HandleDragDropTarget(const FileGridItem& item, const FileGridCallbacks& callbacks);
		
		bool IsInSelectionRectangle(const ImRect& itemBounds) const;
	};

} // namespace Lunex::UI
