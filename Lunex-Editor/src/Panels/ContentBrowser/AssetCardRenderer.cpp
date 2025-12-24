#include "stpch.h"
#include "AssetCardRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Lunex {

	float AssetCardRenderer::GetFileCardHeight(bool isHDR) const {
		float iconHeight = isHDR ? (GetFileCardWidth(true) / 2.0f) : m_Style.ThumbnailSize;
		return iconHeight + 50.0f;
	}

	int AssetCardRenderer::CalculateColumnCount(float panelWidth) const {
		float cellSize = GetCellSize();
		int count = (int)(panelWidth / cellSize);
		return count < 1 ? 1 : count;
	}

	AssetCardRenderer::CardResult AssetCardRenderer::RenderFolderCard(
		const std::filesystem::path& path, const Ref<Texture2D>& icon,
		bool isSelected, bool isHovered) {

		CardResult result;
		std::string filename = path.filename().string();

		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float cardWidth = m_Style.ThumbnailSize;
		float cardHeight = m_Style.ThumbnailSize + 30.0f;
		ImVec2 cardMin = cursorPos;
		ImVec2 cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);

		result.Bounds = ImRect(cardMin, cardMax);

		// Draw icon
		float iconSize = m_Style.ThumbnailSize;
		ImVec2 iconPos = cursorPos;

		if (icon) {
			drawList->AddImage(
				(ImTextureID)(uintptr_t)icon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}

		// Draw name
		float textAreaY = iconPos.y + iconSize + 4.0f;
		std::string displayName = TruncateFilename(filename, 15);

		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			IM_COL32(245, 245, 245, 255),
			displayName.c_str()
		);

		// Invisible button for interaction
		ImGui::SetCursorScreenPos(cardMin);
		ImGui::InvisibleButton(filename.c_str(), ImVec2(cardWidth, cardHeight));

		result.Clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		result.DoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		result.RightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

		// Visual effects
		if (isHovered || ImGui::IsItemHovered()) {
			DrawHoverHighlight(drawList, cardMin, cardMax);
		}

		if (isSelected) {
			DrawSelectionHighlight(drawList, cardMin, cardMax);
		}

		// Drop target highlight for folders
		if (ImGui::IsDragDropActive() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
			DrawDropTargetHighlight(drawList, cardMin, cardMax);
		}

		return result;
	}

	AssetCardRenderer::CardResult AssetCardRenderer::RenderFileCard(
		const std::filesystem::path& path, const Ref<Texture2D>& thumbnail,
		const std::string& typeLabel, ImU32 borderColor,
		bool isSelected, bool isHovered, bool isHDR) {

		CardResult result;
		std::string filename = path.filename().string();

		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float cardWidth = isHDR ? (m_Style.ThumbnailSize * 2.0f) : m_Style.ThumbnailSize;
		float iconHeight = isHDR ? (cardWidth / 2.0f) : m_Style.ThumbnailSize;
		float cardHeight = iconHeight + 50.0f;

		ImVec2 cardMin = cursorPos;
		ImVec2 cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);

		result.Bounds = ImRect(cardMin, cardMax);

		// Draw shadow
		DrawCardShadow(drawList, cardMin, cardMax);

		// Draw card background
		bool showBorder = borderColor != IM_COL32(45, 45, 48, 255);
		DrawCardBackground(drawList, cardMin, cardMax, borderColor, showBorder);

		// Draw icon/thumbnail area
		float iconWidth = cardWidth - (m_Style.CardPadding * 2);
		float actualIconHeight = isHDR ? (iconWidth / 2.0f) : iconWidth;

		ImVec2 iconMin = ImVec2(cursorPos.x + m_Style.CardPadding, cursorPos.y + m_Style.CardPadding);
		ImVec2 iconMax = ImVec2(iconMin.x + iconWidth, iconMin.y + actualIconHeight);

		// Icon background
		ImU32 iconBgColor = IM_COL32(55, 55, 58, 255);
		drawList->AddRectFilled(iconMin, iconMax, iconBgColor, 4.0f);

		// Draw thumbnail
		if (thumbnail) {
			drawList->AddImageRounded(
				(ImTextureID)(uintptr_t)thumbnail->GetRendererID(),
				iconMin,
				iconMax,
				ImVec2(0, 1), ImVec2(1, 0),
				IM_COL32(255, 255, 255, 255),
				4.0f
			);
		}

		// Draw text
		float textAreaY = iconMax.y + 4.0f;

		// Get display name (remove extension for Lunex assets)
		std::string displayName = filename;
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lumat" || extension == ".lumesh" || extension == ".luprefab" ||
			extension == ".luanim" || extension == ".luskel") {
			displayName = path.stem().string();
		}

		int maxChars = isHDR ? 30 : 15;
		displayName = TruncateFilename(displayName, maxChars);

		// Draw name
		float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
		float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + nameOffsetX, textAreaY),
			IM_COL32(245, 245, 245, 255),
			displayName.c_str()
		);

		// Draw type label
		float typeWidth = ImGui::CalcTextSize(typeLabel.c_str()).x;
		float typeOffsetX = (cardWidth - typeWidth) * 0.5f;
		drawList->AddText(
			ImVec2(cursorPos.x + typeOffsetX, textAreaY + 16.0f),
			IM_COL32(128, 128, 132, 255),
			typeLabel.c_str()
		);

		// Invisible button for interaction
		ImGui::SetCursorScreenPos(cardMin);
		ImGui::InvisibleButton(filename.c_str(), ImVec2(cardWidth, cardHeight));

		result.Clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		result.DoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		result.RightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

		// Visual effects
		if (isHovered || ImGui::IsItemHovered()) {
			DrawHoverHighlight(drawList, cardMin, cardMax);
		}

		if (isSelected) {
			DrawSelectionHighlight(drawList, cardMin, cardMax);
		}

		return result;
	}

	void AssetCardRenderer::DrawCardBackground(ImDrawList* drawList, const ImVec2& min,
		const ImVec2& max, ImU32 borderColor, bool showBorder) {
		ImU32 cardBgColor = IM_COL32(45, 45, 48, 255);
		drawList->AddRectFilled(min, max, cardBgColor, m_Style.CardRounding);

		if (showBorder) {
			drawList->AddRect(min, max, borderColor, m_Style.CardRounding, 0, 2.0f);
		}
	}

	void AssetCardRenderer::DrawCardShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) {
		ImVec2 shadowMin = ImVec2(min.x + m_Style.ShadowOffset, min.y + m_Style.ShadowOffset);
		ImVec2 shadowMax = ImVec2(max.x + m_Style.ShadowOffset, max.y + m_Style.ShadowOffset);
		ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(m_Style.ShadowAlpha * 255));
		drawList->AddRectFilled(shadowMin, shadowMax, shadowColor, m_Style.CardRounding);
	}

	void AssetCardRenderer::DrawSelectionHighlight(ImDrawList* drawList, const ImVec2& min,
		const ImVec2& max) {
		ImU32 selectedColor = IM_COL32(66, 150, 250, 255);
		drawList->AddRect(min, max, selectedColor, m_Style.CardRounding, 0, 2.5f);

		ImU32 selectedFill = IM_COL32(66, 150, 250, 40);
		drawList->AddRectFilled(min, max, selectedFill, m_Style.CardRounding);
	}

	void AssetCardRenderer::DrawHoverHighlight(ImDrawList* drawList, const ImVec2& min,
		const ImVec2& max) {
		ImU32 hoverColor = IM_COL32(80, 80, 85, 255);
		drawList->AddRect(min, max, hoverColor, m_Style.CardRounding, 0, 2.0f);
	}

	void AssetCardRenderer::DrawDropTargetHighlight(ImDrawList* drawList, const ImVec2& min,
		const ImVec2& max) {
		ImU32 dropTargetColor = IM_COL32(90, 150, 255, 255);
		drawList->AddRect(min, max, dropTargetColor, m_Style.CardRounding, 0, 3.0f);
	}

	std::string AssetCardRenderer::TruncateFilename(const std::string& filename, int maxChars) const {
		if ((int)filename.length() <= maxChars) {
			return filename;
		}
		return filename.substr(0, maxChars - 2) + "..";
	}

} // namespace Lunex
