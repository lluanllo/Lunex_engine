/**
 * @file AssetCard.cpp
 * @brief Asset Card Component Implementation
 */

#include "stpch.h"
#include "AssetCard.h"

namespace Lunex::UI {

	AssetCardResult AssetCard::Render(const std::string& id, const std::string& name,
									  const std::string& typeLabel, Ref<Texture2D> thumbnail,
									  bool isSelected, bool isDirectory) {
		AssetCardResult result;
		
		ScopedID scopedID(id);
		
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float cardHeight = m_Style.thumbnailHeight + 50.0f;
		
		ImVec2 cardMin = cursorPos;
		ImVec2 cardMax = ImVec2(cursorPos.x + m_Style.width, cursorPos.y + cardHeight);
		
		// Render based on type
		if (isDirectory) {
			RenderDirectory(drawList, cursorPos, thumbnail);
		} else {
			RenderFile(drawList, cardMin, cardMax, thumbnail);
		}
		
		// Render text
		RenderText(drawList, cursorPos, name, typeLabel, isDirectory);
		
		// Invisible button for interaction
		ImGui::SetCursorScreenPos(cardMin);
		ImGui::InvisibleButton(id.c_str(), ImVec2(m_Style.width, cardHeight));
		
		bool isHovered = ImGui::IsItemHovered();
		
		// Render selection/hover effects
		RenderSelectionEffects(drawList, cardMin, cardMax, isSelected, isHovered);
		
		// Collect interaction results
		result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		result.doubleClicked = isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		result.rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		result.selected = isSelected;
		
		// Drag source
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			result.dragStarted = true;
			ImGui::EndDragDropSource();
		}
		
		return result;
	}
	
	void AssetCard::RenderDirectory(ImDrawList* drawList, ImVec2 cursorPos, Ref<Texture2D> thumbnail) {
		float iconSize = m_Style.thumbnailHeight;
		
		if (thumbnail) {
			drawList->AddImage(
				(ImTextureID)(intptr_t)thumbnail->GetRendererID(),
				cursorPos,
				ImVec2(cursorPos.x + iconSize, cursorPos.y + iconSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}
	}
	
	void AssetCard::RenderFile(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, Ref<Texture2D> thumbnail) {
		// Shadow
		if (m_Style.showShadow) {
			ImVec2 shadowOffset(3.0f, 3.0f);
			ImVec2 shadowMin = ImVec2(cardMin.x + shadowOffset.x, cardMin.y + shadowOffset.y);
			ImVec2 shadowMax = ImVec2(cardMax.x + shadowOffset.x, cardMax.y + shadowOffset.y);
			drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 80), m_Style.rounding);
		}
		
		// Card background
		drawList->AddRectFilled(cardMin, cardMax, IM_COL32(45, 45, 48, 255), m_Style.rounding);
		
		// Thumbnail area
		float padding = 8.0f;
		float iconWidth = m_Style.width - (padding * 2);
		float iconHeight = m_Style.thumbnailHeight - (padding * 2);
		
		ImVec2 iconMin = ImVec2(cardMin.x + padding, cardMin.y + padding);
		ImVec2 iconMax = ImVec2(iconMin.x + iconWidth, iconMin.y + iconHeight);
		
		// Icon background
		drawList->AddRectFilled(iconMin, iconMax, IM_COL32(55, 55, 58, 255), 4.0f);
		
		// Thumbnail
		if (thumbnail) {
			drawList->AddImageRounded(
				(ImTextureID)(intptr_t)thumbnail->GetRendererID(),
				iconMin, iconMax,
				ImVec2(0, 1), ImVec2(1, 0),
				IM_COL32(255, 255, 255, 255),
				4.0f
			);
		}
	}
	
	void AssetCard::RenderText(ImDrawList* drawList, ImVec2 cursorPos, const std::string& name,
							   const std::string& typeLabel, bool isDirectory) {
		float textAreaY = cursorPos.y + m_Style.thumbnailHeight + 4.0f;
		
		// Truncate name if too long
		std::string displayName = name;
		const size_t maxChars = 15;
		if (displayName.length() > maxChars) {
			displayName = displayName.substr(0, maxChars - 2) + "..";
		}
		
		// Draw name (centered)
		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (m_Style.width - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			IM_COL32(245, 245, 245, 255),
			displayName.c_str()
		);
		
		// Type label (only for files)
		if (!isDirectory && m_Style.showTypeLabel) {
			float typeWidth = ImGui::CalcTextSize(typeLabel.c_str()).x;
			float typeOffsetX = (m_Style.width - typeWidth) * 0.5f;
			drawList->AddText(
				ImVec2(cursorPos.x + typeOffsetX, textAreaY + 16.0f),
				IM_COL32(128, 128, 132, 255),
				typeLabel.c_str()
			);
		}
	}
	
	void AssetCard::RenderSelectionEffects(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax,
										   bool isSelected, bool isHovered) {
		// Hover effect
		if (isHovered && !isSelected) {
			drawList->AddRect(cardMin, cardMax, IM_COL32(60, 60, 65, 255), m_Style.rounding, 0, 2.0f);
		}
		
		// Selection effect
		if (isSelected) {
			drawList->AddRect(cardMin, cardMax, IM_COL32(66, 150, 250, 255), m_Style.rounding, 0, 2.5f);
			drawList->AddRectFilled(cardMin, cardMax, IM_COL32(66, 150, 250, 40), m_Style.rounding);
		}
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	AssetCardResult RenderAssetCard(const std::string& id, const std::string& name,
									const std::string& typeLabel, Ref<Texture2D> thumbnail,
									bool isSelected, bool isDirectory,
									const AssetCardStyle& style) {
		AssetCard card;
		card.SetStyle(style);
		return card.Render(id, name, typeLabel, thumbnail, isSelected, isDirectory);
	}

} // namespace Lunex::UI
