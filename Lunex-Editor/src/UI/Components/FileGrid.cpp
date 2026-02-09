/**
 * @file FileGrid.cpp
 * @brief File Grid Component Implementation
 */

#include "stpch.h"
#include "FileGrid.h"
#include "AssetCard.h"  // For AssetCard::GetTypeColor
#include "../../Panels/ContentBrowserPanel.h"  // For ContentBrowserPayload
#include <sstream>

namespace Lunex::UI {

	FileGridResult FileGrid::Render(const std::vector<FileGridItem>& items,
									std::set<std::string>& selectedItems,
									const FileGridCallbacks& callbacks) {
		FileGridResult result;
		m_ItemBounds.clear();
		
		ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
		
		// Track hover state
		result.hoveredFolder.clear();
		
		// Handle selection rectangle start
		bool isCtrlDown = ImGui::GetIO().KeyCtrl;
		bool isShiftDown = ImGui::GetIO().KeyShift;
		
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
			!isCtrlDown && !isShiftDown && !ImGui::IsAnyItemHovered()) {
			m_IsSelecting = true;
			m_SelectionStart = ImGui::GetMousePos();
			m_SelectionEnd = m_SelectionStart;
			selectedItems.clear();
		}
		
		// Update selection rectangle
		if (m_IsSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_SelectionEnd = ImGui::GetMousePos();
		}
		
		// End selection rectangle
		if (m_IsSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			m_IsSelecting = false;
		}
		
		// Context menu on empty space
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) {
			if (callbacks.onEmptySpaceRightClicked) {
				callbacks.onEmptySpaceRightClicked();
			}
		}
		
		result.isSelecting = m_IsSelecting;
		
		// Calculate grid layout
		float cellSize = m_Style.thumbnailSize + m_Style.padding * 2;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;
		
		// Need at least 2 columns for HDR cards
		if (columnCount < 2) columnCount = 2;
		
		ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(m_Style.padding, m_Style.padding + 8));
		
		// Manual grid rendering to handle HDR cards spanning 2 columns
		int currentColumn = 0;
		int currentRow = 0;
		ImVec2 startPos = ImGui::GetCursorScreenPos();
		float rowHeight = 0.0f;
		float rowStartY = startPos.y;
		
		ScopedColor colors({
			{ImGuiCol_Button, Color(0, 0, 0, 0)},
			{ImGuiCol_ButtonHovered, Color(0.16f, 0.16f, 0.16f, 0.6f)},
			{ImGuiCol_ButtonActive, Color(0.91f, 0.57f, 0.18f, 0.50f)}
		});
		
		for (const auto& item : items) {
			int cardSpan = item.isHDR ? 2 : 1;
			
			// Check if we need to move to next row
			if (currentColumn + cardSpan > columnCount) {
				currentColumn = 0;
				rowStartY += rowHeight + m_Style.padding + 8;
				rowHeight = 0.0f;
			}
			
			// Calculate position
			float x = startPos.x + currentColumn * cellSize;
			float y = rowStartY;
			
			ImGui::SetCursorScreenPos(ImVec2(x, y));
			
			ScopedID itemID(item.name);
			ImGui::BeginGroup();
			
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			
			float cardWidth = GetCardWidth(item);
			float cardHeight = GetCardHeight(item);
			ImVec2 cardMin = cursorPos;
			ImVec2 cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);
			
			// Update row height
			if (cardHeight > rowHeight) {
				rowHeight = cardHeight;
			}
			
			// Render card based on type
			if (item.isDirectory) {
				RenderFolderCard(item, cursorPos, drawList);
			} else {
				RenderFileCard(item, cursorPos, drawList);
			}
			
			// Invisible button for interaction
			ImGui::SetCursorScreenPos(cardMin);
			ImGui::InvisibleButton(item.name.c_str(), ImVec2(cardWidth, cardHeight));
			
			// Store bounds for selection rectangle
			m_ItemBounds[item.path.string()] = ImRect(cardMin, cardMax);
			
			// Check selection rectangle
			if (m_IsSelecting && IsInSelectionRectangle(m_ItemBounds[item.path.string()])) {
				selectedItems.insert(item.path.string());
			}
			
			bool isSelected = selectedItems.find(item.path.string()) != selectedItems.end();
			bool isHovered = ImGui::IsItemHovered();
			bool isDropTarget = item.isDirectory && ImGui::IsDragDropActive() && 
								ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			
			// Selection effects
			RenderSelectionEffects(cardMin, cardMax, isSelected, isHovered, isDropTarget);
			
			// Track hovered folder
			if (item.isDirectory && isHovered) {
				result.hoveredFolder = item.path;
			}
			
			// Click handling
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				result.clickedItem = item.path;
				if (callbacks.onItemClicked) {
					callbacks.onItemClicked(item.path);
				}
				m_IsSelecting = false;
			}
			
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				result.doubleClickedItem = item.path;
				if (callbacks.onItemDoubleClicked) {
					callbacks.onItemDoubleClicked(item.path);
				}
			}
			
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				result.rightClickedItem = item.path;
				if (callbacks.onItemRightClicked) {
					callbacks.onItemRightClicked(item.path);
				}
			}
			
			// Drag & drop
			HandleDragDropSource(item, selectedItems);
			
			if (item.isDirectory) {
				HandleDragDropTarget(item, callbacks);
			}
			
			ImGui::EndGroup();
			
			currentColumn += cardSpan;
		}
		
		// Reserve space for the grid
		float totalHeight = rowStartY + rowHeight - startPos.y;
		ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + totalHeight + m_Style.padding));
		ImGui::Dummy(ImVec2(0, 0));
		
		// Deselect on empty space click
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!ImGui::IsAnyItemHovered() && !isCtrlDown && !isShiftDown && !m_IsSelecting) {
			result.clickedEmptySpace = true;
			if (callbacks.onEmptySpaceClicked) {
				callbacks.onEmptySpaceClicked();
			}
		}
		
		// Draw selection rectangle
		if (m_IsSelecting) {
			RenderSelectionRectangle();
		}
		
		return result;
	}
	
	void FileGrid::RenderFolderCard(const FileGridItem& item, ImVec2 cursorPos, ImDrawList* drawList) {
		float iconSize = m_Style.thumbnailSize;
		
		// Folder icon (no background card)
		if (item.thumbnail) {
			drawList->AddImage(
				(ImTextureID)(intptr_t)item.thumbnail->GetRendererID(),
				cursorPos,
				ImVec2(cursorPos.x + iconSize, cursorPos.y + iconSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}
		
		// Folder name
		float textAreaY = cursorPos.y + iconSize + 4.0f;
		std::string displayName = item.name;
		
		const int maxChars = 15;
		if (displayName.length() > maxChars) {
			displayName = displayName.substr(0, maxChars - 2) + "..";
		}
		
		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (m_Style.thumbnailSize - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.textColor)),
			displayName.c_str()
		);
	}
	
	void FileGrid::RenderFileCard(const FileGridItem& item, ImVec2 cursorPos, ImDrawList* drawList) {
		float cardWidth = GetCardWidth(item);
		float cardHeight = GetCardHeight(item);
		ImVec2 cardMin = cursorPos;
		ImVec2 cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);
		
		// Shadow
		ImVec2 shadowOffset(3.0f, 3.0f);
		ImVec2 shadowMin = ImVec2(cardMin.x + shadowOffset.x, cardMin.y + shadowOffset.y);
		ImVec2 shadowMax = ImVec2(cardMax.x + shadowOffset.x, cardMax.y + shadowOffset.y);
		drawList->AddRectFilled(shadowMin, shadowMax, 
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.shadowColor)), 
			m_Style.cardRounding);
		
		// Card background
		drawList->AddRectFilled(cardMin, cardMax, 
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.cardBgColor)), 
			m_Style.cardRounding);
		
		// Thumbnail area
		float iconWidth = cardWidth - (m_Style.cardPadding * 2);
		float iconHeight;
		
		if (item.isHDR) {
			// HDR: wide aspect ratio
			iconHeight = (cardWidth - (m_Style.cardPadding * 2)) / 2.0f;
		} else {
			// Square thumbnails
			iconHeight = m_Style.thumbnailSize - (m_Style.cardPadding * 2);
		}
		
		ImVec2 iconMin = ImVec2(cursorPos.x + m_Style.cardPadding, cursorPos.y + m_Style.cardPadding);
		ImVec2 iconMax = ImVec2(iconMin.x + iconWidth, iconMin.y + iconHeight);
		
		// Icon background
		drawList->AddRectFilled(iconMin, iconMax, 
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.iconBgColor)), 
			4.0f);
		
		// Thumbnail
		if (item.thumbnail) {
			drawList->AddImageRounded(
				(ImTextureID)(intptr_t)item.thumbnail->GetRendererID(),
				iconMin, iconMax,
				ImVec2(0, 1), ImVec2(1, 0),
				IM_COL32(255, 255, 255, 255),
				4.0f
			);
		}
		
		// Type border (bottom of card)
		Color typeColor = AssetCard::GetTypeColor(item.typeLabel);
		if (typeColor.a > 0.01f) {
			float borderThickness = 2.5f;
			ImVec2 borderMin = ImVec2(cardMin.x, cardMax.y - borderThickness - 1);
			ImVec2 borderMax = ImVec2(cardMax.x, cardMax.y - 1);
			ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(ToImVec4(typeColor));
			drawList->AddRectFilled(borderMin, borderMax, borderColorU32, 
				m_Style.cardRounding, ImDrawFlags_RoundCornersBottom);
		}
		
		// Text area
		float textAreaY = iconMax.y + 4.0f;
		
		// Asset name
		std::string displayName = item.name;
		
		// Truncate if needed (adjust for card width)
		size_t maxChars = (size_t)(cardWidth / 7.0f);
		maxChars = maxChars < 8 ? 8 : maxChars;
		
		if (displayName.length() > maxChars) {
			displayName = displayName.substr(0, maxChars - 2) + "..";
		}
		
		// Center name
		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.textColor)),
			displayName.c_str()
		);
		
		// Type label with color
		float typeWidth = ImGui::CalcTextSize(item.typeLabel.c_str()).x;
		float typeOffsetX = (cardWidth - typeWidth) * 0.5f;
		
		// Use muted type color
		ImU32 typeLabelColor = IM_COL32(
			(int)(typeColor.r * 180), 
			(int)(typeColor.g * 180), 
			(int)(typeColor.b * 180), 
			200
		);
		
		drawList->AddText(
			ImVec2(cursorPos.x + typeOffsetX, textAreaY + 16.0f),
			typeLabelColor,
			item.typeLabel.c_str()
		);
	}
	
	void FileGrid::RenderSelectionEffects(ImVec2 cardMin, ImVec2 cardMax, 
										  bool isSelected, bool isHovered, bool isDropTarget) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		if (isDropTarget) {
			ImU32 dropColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.dropTargetColor));
			drawList->AddRect(cardMin, cardMax, dropColor, m_Style.cardRounding, 0, 3.0f);
		}
		else if (isSelected) {
			ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.selectedBorderColor));
			ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.selectedFillColor));
			drawList->AddRect(cardMin, cardMax, borderColor, m_Style.cardRounding, 0, 2.5f);
			drawList->AddRectFilled(cardMin, cardMax, fillColor, m_Style.cardRounding);
		}
		else if (isHovered) {
			ImU32 hoverColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.hoverColor));
			drawList->AddRect(cardMin, cardMax, hoverColor, m_Style.cardRounding, 0, 2.0f);
		}
	}
	
	void FileGrid::RenderSelectionRectangle() {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		ImVec2 rectMin(
			(std::min)(m_SelectionStart.x, m_SelectionEnd.x),
			(std::min)(m_SelectionStart.y, m_SelectionEnd.y)
		);
		ImVec2 rectMax(
			(std::max)(m_SelectionStart.x, m_SelectionEnd.x),
			(std::max)(m_SelectionStart.y, m_SelectionEnd.y)
		);
		
		ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.selectionRectFill));
		ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ToImVec4(m_Style.selectionRectBorder));
		
		drawList->AddRectFilled(rectMin, rectMax, fillColor);
		drawList->AddRect(rectMin, rectMax, borderColor, 0.0f, 0, 2.0f);
	}
	
	float FileGrid::GetCardWidth(const FileGridItem& item) const {
		if (item.isDirectory) {
			return m_Style.thumbnailSize;
		}
		// HDR cards are double width
		return item.isHDR ? (m_Style.thumbnailSize * 2.0f + m_Style.padding) : m_Style.thumbnailSize;
	}
	
	float FileGrid::GetCardHeight(const FileGridItem& item) const {
		if (item.isDirectory) {
			return m_Style.thumbnailSize + 30.0f;
		}
		
		if (item.isHDR) {
			// HDR: calculate based on 2:1 thumbnail ratio
			float cardWidth = m_Style.thumbnailSize * 2.0f + m_Style.padding;
			float iconWidth = cardWidth - (m_Style.cardPadding * 2);
			float iconHeight = iconWidth / 2.0f;
			return iconHeight + (m_Style.cardPadding * 2) + 50.0f;
		}
		
		return m_Style.thumbnailSize + 50.0f;
	}
	
	void FileGrid::HandleDragDropSource(const FileGridItem& item, const std::set<std::string>& selectedItems) {
		if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
		
		bool isSelected = selectedItems.find(item.path.string()) != selectedItems.end();
		
		if (isSelected && selectedItems.size() > 1) {
			// Multiple items payload
			std::string payloadData;
			for (const auto& path : selectedItems) {
				std::filesystem::path p(path);
				payloadData += p.filename().string() + "\n";
			}
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEMS", payloadData.c_str(), payloadData.size() + 1);
			ImGui::Text("%d items", (int)selectedItems.size());
		}
		else {
			// Single item payload - use canonical ContentBrowserPayload struct
			Lunex::ContentBrowserPayload payload = {};
			strncpy_s(payload.FilePath, item.path.string().c_str(), _TRUNCATE);
			
			std::string ext = item.path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			strncpy_s(payload.Extension, ext.c_str(), _TRUNCATE);
			
			payload.IsDirectory = item.isDirectory;
			payload.ItemCount = 1;
			
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &payload, sizeof(payload));
			ImGui::Text("%s", item.name.c_str());
		}
		
		ImGui::EndDragDropSource();
	}
	
	void FileGrid::HandleDragDropTarget(const FileGridItem& item, const FileGridCallbacks& callbacks) {
		if (!ImGui::BeginDragDropTarget()) return;
		
		// Single item
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			if (callbacks.onItemDropped) {
				callbacks.onItemDropped(item.path, payload->Data, payload->DataSize);
			}
		}
		
		// Multiple items
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEMS")) {
			if (callbacks.onMultiItemDropped) {
				std::string data((const char*)payload->Data);
				callbacks.onMultiItemDropped(item.path, data);
			}
		}
		
		ImGui::EndDragDropTarget();
	}
	
	bool FileGrid::IsInSelectionRectangle(const ImRect& itemBounds) const {
		ImRect selectionRect(
			ImVec2((std::min)(m_SelectionStart.x, m_SelectionEnd.x),
				   (std::min)(m_SelectionStart.y, m_SelectionEnd.y)),
			ImVec2((std::max)(m_SelectionStart.x, m_SelectionEnd.x),
				   (std::max)(m_SelectionStart.y, m_SelectionEnd.y))
		);
		return selectionRect.Overlaps(itemBounds);
	}

} // namespace Lunex::UI
