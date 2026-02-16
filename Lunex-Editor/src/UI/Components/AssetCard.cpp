/**
 * @file AssetCard.cpp
 * @brief Asset Card Component Implementation
 */

#include "stpch.h"
#include "AssetCard.h"

namespace Lunex::UI {

	Color AssetCard::GetTypeColor(const std::string& typeLabel) {
		if (typeLabel == "MESH" || typeLabel == "LUMESH") return AssetTypeColors::Mesh();
		if (typeLabel == "PREFAB" ) return AssetTypeColors::Prefab();
		if (typeLabel == "MATERIAL") return AssetTypeColors::Material();
		if (typeLabel == "HDRI" || typeLabel == "HDR") return AssetTypeColors::HDR();
		if (typeLabel == "TEXTURE") return AssetTypeColors::Texture();
		if (typeLabel == "SCENE" ) return AssetTypeColors::Scene();
		if (typeLabel == "SCRIPT" ) return AssetTypeColors::Script();
		if (typeLabel == "AUDIO"  ) return AssetTypeColors::Audio();
		if (typeLabel == "SHADER" ) return AssetTypeColors::Shader();
		if (typeLabel == "FOLDER" ) return AssetTypeColors::Folder();
		return AssetTypeColors::Default();
	}

	AssetCardResult AssetCard::Render(const std::string& id, const std::string& name,
									  const std::string& typeLabel, Ref<Texture2D> thumbnail,
									  bool isSelected, bool isDirectory, bool isWideAspect) {
		AssetCardResult result;
		
		ScopedID scopedID(id);
		
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Calculate card dimensions
		float cardWidth = isWideAspect ? (m_Style.width * 2.0f + 12.0f) : m_Style.width;  // Double width + padding for HDR
		float cardHeight = m_Style.thumbnailHeight + 50.0f;
		
		ImVec2 cardMin = cursorPos;
		ImVec2 cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);
		
		// Render based on type
		if (isDirectory) {
			RenderDirectory(drawList, cursorPos, thumbnail);
		} else {
			RenderFile(drawList, cardMin, cardMax, thumbnail, isWideAspect);
		}
		
		// Render text
		RenderText(drawList, cursorPos, cardWidth, name, typeLabel, isDirectory);
		
		// Invisible button for interaction
		ImGui::SetCursorScreenPos(cardMin);
		ImGui::InvisibleButton(id.c_str(), ImVec2(cardWidth, cardHeight));
		
		bool isHovered = ImGui::IsItemHovered();
		
		// Render type border (before selection effects so selection overlays it)
		if (!isDirectory && m_Style.showTypeBorder) {
			RenderTypeBorder(drawList, cardMin, cardMax, typeLabel, isDirectory);
		}
		
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
		
		if (thumbnail && thumbnail->GetRendererID() != 0) {
			drawList->AddImage(
				(ImTextureID)(intptr_t)thumbnail->GetRendererID(),
				cursorPos,
				ImVec2(cursorPos.x + iconSize, cursorPos.y + iconSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}
	}
	
	void AssetCard::RenderFile(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, Ref<Texture2D> thumbnail, bool isWideAspect) {
		// Shadow
		if (m_Style.showShadow) {
			ImVec2 shadowOffset(2.0f, 2.0f);
			ImVec2 shadowMin = ImVec2(cardMin.x + shadowOffset.x, cardMin.y + shadowOffset.y);
			ImVec2 shadowMax = ImVec2(cardMax.x + shadowOffset.x, cardMax.y + shadowOffset.y);
			drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 50), m_Style.rounding);
		}
		
		// Card background (blue-tinted dark)
		drawList->AddRectFilled(cardMin, cardMax, Colors::BgCard().ToImU32(), m_Style.rounding);
		
		// Thumbnail area
		float padding = 8.0f;
		float cardWidth = cardMax.x - cardMin.x;
		float iconWidth = cardWidth - (padding * 2);
		float iconHeight = m_Style.thumbnailHeight - (padding * 2);
		
		ImVec2 iconMin = ImVec2(cardMin.x + padding, cardMin.y + padding);
		ImVec2 iconMax = ImVec2(iconMin.x + iconWidth, iconMin.y + iconHeight);
		
		// Icon background (blue-tinted dark)
		drawList->AddRectFilled(iconMin, iconMax, Colors::BgDark().ToImU32(), 3.0f);
		
		// Thumbnail
		if (thumbnail && thumbnail->GetRendererID() != 0) {
			// Calculate UV coordinates to properly fit the thumbnail
			ImVec2 uv0(0, 1);
			ImVec2 uv1(1, 0);
			
			// For HDR/wide images, we want to show the full image in the wide card
			// For regular images, we maintain aspect ratio and center
			if (!isWideAspect && thumbnail->GetWidth() > 0 && thumbnail->GetHeight() > 0) {
				float texAspect = (float)thumbnail->GetWidth() / (float)thumbnail->GetHeight();
				float cardAspect = iconWidth / iconHeight;
				
				if (texAspect > cardAspect) {
					// Texture is wider than card - crop sides
					float uvWidth = cardAspect / texAspect;
					float uvOffset = (1.0f - uvWidth) * 0.5f;
					uv0 = ImVec2(uvOffset, 1);
					uv1 = ImVec2(uvOffset + uvWidth, 0);
				} else {
					// Texture is taller than card - crop top/bottom
					float uvHeight = texAspect / cardAspect;
					float uvOffset = (1.0f - uvHeight) * 0.5f;
					uv0 = ImVec2(0, 1 - uvOffset);
					uv1 = ImVec2(1, uvOffset);
				}
			}
			
			drawList->AddImageRounded(
				(ImTextureID)(intptr_t)thumbnail->GetRendererID(),
				iconMin, iconMax,
				uv0, uv1,
				IM_COL32(255, 255, 255, 255),
				4.0f
			);
		}
	}
	
	void AssetCard::RenderText(ImDrawList* drawList, ImVec2 cursorPos, float cardWidth, const std::string& name,
							   const std::string& typeLabel, bool isDirectory) {
		float textAreaY = cursorPos.y + m_Style.thumbnailHeight + 4.0f;
		
		// Truncate name if too long (adjust for card width)
		std::string displayName = name;
		size_t maxChars = (size_t)(cardWidth / 7.0f);  // Approximate chars that fit
		maxChars = maxChars < 8 ? 8 : maxChars;
		
		if (displayName.length() > maxChars) {
			displayName = displayName.substr(0, maxChars - 2) + "..";
		}
		
		// Draw name (centered, using theme text color)
		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			Colors::TextPrimary().ToImU32(),
			displayName.c_str()
		);
		
		// Type label (only for files)
		if (!isDirectory && m_Style.showTypeLabel) {
			float typeWidth = ImGui::CalcTextSize(typeLabel.c_str()).x;
			float typeOffsetX = (cardWidth - typeWidth) * 0.5f;
			
			// Use type color for the label text
			Color typeColor = GetTypeColor(typeLabel);
			ImU32 typeColorU32 = IM_COL32(
				(int)(typeColor.r * 170), 
				(int)(typeColor.g * 170), 
				(int)(typeColor.b * 170), 
				180
			);
			
			drawList->AddText(
				ImVec2(cursorPos.x + typeOffsetX, textAreaY + 16.0f),
				typeColorU32,
				typeLabel.c_str()
			);
		}
	}
	
	void AssetCard::RenderTypeBorder(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax,
									 const std::string& typeLabel, bool isDirectory) {
		if (isDirectory) return;
		
		Color borderColor = GetTypeColor(typeLabel);
		if (borderColor.a < 0.01f) return;  // Skip if transparent
		
		ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(ToImVec4(borderColor));
		
		// Draw subtle border at the bottom of the card
		float borderThickness = m_Style.typeBorderWidth;
		ImVec2 borderMin = ImVec2(cardMin.x, cardMax.y - borderThickness - 1);
		ImVec2 borderMax = ImVec2(cardMax.x, cardMax.y - 1);
		
		drawList->AddRectFilled(borderMin, borderMax, borderColorU32, m_Style.rounding, ImDrawFlags_RoundCornersBottom);
	}
	
	void AssetCard::RenderSelectionEffects(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax,
										   bool isSelected, bool isHovered) {
		// Hover effect (blue-tinted)
		if (isHovered && !isSelected) {
			drawList->AddRect(cardMin, cardMax, IM_COL32(55, 68, 85, 200), m_Style.rounding, 0, 1.0f);
		}
		
		// Selection effect (teal accent)
		if (isSelected) {
			Color sel = Colors::SelectedBorder();
			ImU32 borderCol = IM_COL32((int)(sel.r * 255), (int)(sel.g * 255), (int)(sel.b * 255), 220);
			ImU32 fillCol = IM_COL32((int)(sel.r * 255), (int)(sel.g * 255), (int)(sel.b * 255), 25);
			drawList->AddRect(cardMin, cardMax, borderCol, m_Style.rounding, 0, 2.0f);
			drawList->AddRectFilled(cardMin, cardMax, fillCol, m_Style.rounding);
		}
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	AssetCardResult RenderAssetCard(const std::string& id, const std::string& name,
									const std::string& typeLabel, Ref<Texture2D> thumbnail,
									bool isSelected, bool isDirectory, bool isWideAspect,
									const AssetCardStyle& style) {
		AssetCard card;
		card.SetStyle(style);
		return card.Render(id, name, typeLabel, thumbnail, isSelected, isDirectory, isWideAspect);
	}

} // namespace Lunex::UI
