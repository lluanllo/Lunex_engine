/**
 * @file AssetCard.h
 * @brief Asset Card Component for Content Browser
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// ASSET CARD COMPONENT
	// ============================================================================
	
	struct AssetCardStyle {
		float width = 100.0f;
		float thumbnailHeight = 80.0f;
		float rounding = SpacingValues::CardRounding;
		bool showShadow = true;
		bool showTypeLabel = true;
	};
	
	struct AssetCardResult {
		bool clicked = false;
		bool doubleClicked = false;
		bool rightClicked = false;
		bool dragStarted = false;
		bool selected = false;
	};
	
	/**
	 * @class AssetCard
	 * @brief Renders an asset card for content browser grids
	 * 
	 * Features:
	 * - Thumbnail preview
	 * - Asset name (truncated if too long)
	 * - Type label
	 * - Selection highlight
	 * - Hover effects
	 * - Drag & drop source
	 */
	class AssetCard {
	public:
		AssetCard() = default;
		~AssetCard() = default;
		
		/**
		 * @brief Render the asset card
		 * @param id Unique identifier
		 * @param name Asset display name
		 * @param typeLabel Type description (e.g., "Material", "Texture")
		 * @param thumbnail Preview texture
		 * @param isSelected Whether the card is selected
		 * @param isDirectory Whether this is a folder
		 * @return Interaction result
		 */
		AssetCardResult Render(const std::string& id,
							   const std::string& name,
							   const std::string& typeLabel,
							   Ref<Texture2D> thumbnail,
							   bool isSelected = false,
							   bool isDirectory = false);
		
		// Style configuration
		void SetStyle(const AssetCardStyle& style) { m_Style = style; }
		AssetCardStyle& GetStyle() { return m_Style; }
		const AssetCardStyle& GetStyle() const { return m_Style; }
		
		// Individual style setters
		void SetWidth(float width) { m_Style.width = width; }
		void SetThumbnailHeight(float height) { m_Style.thumbnailHeight = height; }
		void SetRounding(float rounding) { m_Style.rounding = rounding; }
		void SetShowShadow(bool show) { m_Style.showShadow = show; }
		void SetShowTypeLabel(bool show) { m_Style.showTypeLabel = show; }
		
	private:
		AssetCardStyle m_Style;
		
		void RenderDirectory(ImDrawList* drawList, ImVec2 cursorPos, Ref<Texture2D> thumbnail);
		void RenderFile(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, Ref<Texture2D> thumbnail);
		void RenderText(ImDrawList* drawList, ImVec2 cursorPos, const std::string& name, const std::string& typeLabel, bool isDirectory);
		void RenderSelectionEffects(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, bool isSelected, bool isHovered);
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION (for backwards compatibility)
	// ============================================================================
	
	/**
	 * @brief Static function to render an asset card (backwards compatible)
	 */
	AssetCardResult RenderAssetCard(const std::string& id,
									const std::string& name,
									const std::string& typeLabel,
									Ref<Texture2D> thumbnail,
									bool isSelected = false,
									bool isDirectory = false,
									const AssetCardStyle& style = AssetCardStyle());

} // namespace Lunex::UI
