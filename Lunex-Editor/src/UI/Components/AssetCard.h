/**
 * @file AssetCard.h
 * @brief Asset Card Component for Content Browser
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// ASSET TYPE COLORS
	// ============================================================================
	
	namespace AssetTypeColors {
		// Border colors per asset type: one distinct hue each, matching the icon color
		inline Color Scene()     { return Color(0.95f, 0.82f, 0.28f, 1.0f); }  // Yellow
		inline Color Material()  { return Color(0.95f, 0.58f, 0.20f, 1.0f); }  // Orange
		inline Color Texture()   { return Color(0.92f, 0.42f, 0.36f, 1.0f); }  // Coral
		inline Color Audio()     { return Color(0.88f, 0.36f, 0.62f, 1.0f); }  // Pink
		inline Color Particle()  { return Color(0.76f, 0.34f, 0.80f, 1.0f); }  // Magenta
		inline Color Skeleton()  { return Color(0.58f, 0.36f, 0.88f, 1.0f); }  // Purple
		inline Color Shader()    { return Color(0.60f, 0.56f, 0.92f, 1.0f); }  // Lavender
		inline Color Script()    { return Color(0.26f, 0.52f, 0.90f, 1.0f); }  // Blue
		inline Color Blueprint() { return Color(0.22f, 0.40f, 0.76f, 1.0f); }  // Navy
		inline Color HDR()       { return Color(0.30f, 0.36f, 0.80f, 1.0f); }  // Indigo
		inline Color Prefab()    { return Color(0.25f, 0.74f, 0.92f, 1.0f); }  // Cyan
		inline Color Animation() { return Color(0.16f, 0.72f, 0.64f, 1.0f); }  // Teal
		inline Color Mesh()      { return Color(0.30f, 0.78f, 0.40f, 1.0f); }  // Green
		inline Color Default()   { return Color(0.50f, 0.50f, 0.55f, 1.0f); }  // Gray
		inline Color Folder()   { return Color(0.0f, 0.0f, 0.0f, 0.0f); }     // Transparent (no border)
	}

	// ============================================================================
	// ASSET CARD COMPONENT
	// ============================================================================
	
	struct AssetCardStyle {
		float width = 100.0f;
		float thumbnailHeight = 80.0f;
		float rounding = SpacingValues::CardRounding;
		bool showShadow = true;
		bool showTypeLabel = true;
		bool showTypeBorder = true;       // Show colored border based on asset type
		float typeBorderWidth = 2.5f;     // Border width for type indicator
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
	 * - Colored type border
	 * - Selection highlight
	 * - Hover effects
	 * - Drag & drop source
	 * - Wide aspect ratio support for HDR images
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
		 * @param isWideAspect Whether to use wide (2:1) aspect ratio for thumbnail
		 * @return Interaction result
		 */
		AssetCardResult Render(const std::string& id,
							   const std::string& name,
							   const std::string& typeLabel,
							   Ref<Texture2D> thumbnail,
							   bool isSelected = false,
							   bool isDirectory = false,
							   bool isWideAspect = false);
		
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
		void SetShowTypeBorder(bool show) { m_Style.showTypeBorder = show; }
		
		/**
		 * @brief Get the color for a given asset type label
		 */
		static Color GetTypeColor(const std::string& typeLabel);
		
	private:
		AssetCardStyle m_Style;
		
		void RenderDirectory(ImDrawList* drawList, ImVec2 cursorPos, Ref<Texture2D> thumbnail);
		void RenderFile(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, Ref<Texture2D> thumbnail, bool isWideAspect);
		void RenderText(ImDrawList* drawList, ImVec2 cursorPos, float cardWidth, const std::string& name, const std::string& typeLabel, bool isDirectory);
		void RenderSelectionEffects(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, bool isSelected, bool isHovered);
		void RenderTypeBorder(ImDrawList* drawList, ImVec2 cardMin, ImVec2 cardMax, const std::string& typeLabel, bool isDirectory);
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
									bool isWideAspect = false,
									const AssetCardStyle& style = AssetCardStyle());

} // namespace Lunex::UI
