#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>

namespace Lunex {

	class ThumbnailManager;

	// ============================================================================
	// ASSET CARD RENDERER - Renders individual asset cards in the content browser
	// ============================================================================
	class AssetCardRenderer {
	public:
		struct CardStyle {
			float ThumbnailSize = 96.0f;
			float Padding = 12.0f;
			float CardRounding = 6.0f;
			float CardPadding = 8.0f;
			float ShadowOffset = 3.0f;
			float ShadowAlpha = 0.3f;
		};

		struct CardResult {
			bool Clicked = false;
			bool DoubleClicked = false;
			bool RightClicked = false;
			bool DragStarted = false;
			ImRect Bounds;
		};

		AssetCardRenderer() = default;
		~AssetCardRenderer() = default;

		void SetStyle(const CardStyle& style) { m_Style = style; }
		const CardStyle& GetStyle() const { return m_Style; }
		void SetThumbnailSize(float size) { m_Style.ThumbnailSize = size; }

		// Render a folder card (no card background, just icon + name)
		CardResult RenderFolderCard(const std::filesystem::path& path,
			const Ref<Texture2D>& icon, bool isSelected, bool isHovered);

		// Render a file card (with card background, shadow, border)
		CardResult RenderFileCard(const std::filesystem::path& path,
			const Ref<Texture2D>& thumbnail, const std::string& typeLabel,
			ImU32 borderColor, bool isSelected, bool isHovered, bool isHDR = false);

		// Get card dimensions
		float GetFolderCardWidth() const { return m_Style.ThumbnailSize; }
		float GetFolderCardHeight() const { return m_Style.ThumbnailSize + 30.0f; }
		float GetFileCardWidth(bool isHDR = false) const { 
			return isHDR ? m_Style.ThumbnailSize * 2.0f : m_Style.ThumbnailSize; 
		}
		float GetFileCardHeight(bool isHDR = false) const;
		float GetCellSize() const { return m_Style.ThumbnailSize + m_Style.Padding * 2; }

		// Calculate grid layout
		int CalculateColumnCount(float panelWidth) const;

	private:
		void DrawCardBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max,
			ImU32 borderColor, bool showBorder);
		void DrawCardShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max);
		void DrawSelectionHighlight(ImDrawList* drawList, const ImVec2& min, const ImVec2& max);
		void DrawHoverHighlight(ImDrawList* drawList, const ImVec2& min, const ImVec2& max);
		void DrawDropTargetHighlight(ImDrawList* drawList, const ImVec2& min, const ImVec2& max);

		std::string TruncateFilename(const std::string& filename, int maxChars) const;

	private:
		CardStyle m_Style;
	};

} // namespace Lunex
