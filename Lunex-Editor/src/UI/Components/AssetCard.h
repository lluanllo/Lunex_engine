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
		// Border colors for different asset types
		inline Color Mesh()     { return Color(0.30f, 0.85f, 0.40f, 1.0f); }  // Green
		inline Color Prefab()   { return Color(0.40f, 0.75f, 0.95f, 1.0f); }  // Light Blue
		inline Color Material() { return Color(0.95f, 0.60f, 0.20f, 1.0f); }  // Orange
		inline Color HDR()      { return Color(0.25f, 0.45f, 0.85f, 1.0f); }  // Dark Blue
		inline Color Texture()  { return Color(0.80f, 0.50f, 0.80f, 1.0f); }  // Purple
		inline Color Scene()    { return Color(0.95f, 0.85f, 0.30f, 1.0f); }  // Yellow
		inline Color Script()   { return Color(0.50f, 0.80f, 0.50f, 1.0f); }  // Soft Green
		inline Color Audio()    { return Color(0.85f, 0.45f, 0.55f, 1.0f); }  // Pink
		inline Color Shader()   { return Color(0.70f, 0.70f, 0.85f, 1.0f); }  // Lavender
		inline Color Default()  { return Color(0.50f, 0.50f, 0.55f, 1.0f); }  // Gray
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
