/**
 * @file MaterialPreview.h
 * @brief Material Preview Component for Properties Panel
 */

#pragma once

#include "../UICore.h"
#include "../UIComponents.h"
#include "../UILayout.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// MATERIAL PREVIEW COMPONENT
	// ============================================================================
	
	struct MaterialPreviewStyle {
		Size size = Size(70, 70);
		float rounding = 3.0f;
		Color borderColor = Color::FromHex(0x0E1218);
		bool showBorder = true;
	};
	
	struct MaterialPreviewResult {
		bool editClicked = false;
		bool removeClicked = false;
		bool resetClicked = false;
	};
	
	/**
	 * @class MaterialPreview
	 * @brief Renders a material preview card with actions
	 * 
	 * Features:
	 * - Thumbnail preview
	 * - Material name and path
	 * - Override indicator
	 * - Edit and reset buttons
	 */
	class MaterialPreview {
	public:
		MaterialPreview() = default;
		~MaterialPreview() = default;
		
		/**
		 * @brief Render the material preview
		 * @param id Unique identifier
		 * @param materialName Material display name
		 * @param thumbnail Preview texture
		 * @param hasOverrides Whether material has local overrides
		 * @param assetPath Path to the material asset
		 * @return Interaction result
		 */
		MaterialPreviewResult Render(const std::string& id,
									 const std::string& materialName,
									 Ref<Texture2D> thumbnail,
									 bool hasOverrides = false,
									 const std::string& assetPath = "");
		
		// Style configuration
		void SetStyle(const MaterialPreviewStyle& style) { m_Style = style; }
		MaterialPreviewStyle& GetStyle() { return m_Style; }
		const MaterialPreviewStyle& GetStyle() const { return m_Style; }
		
	private:
		MaterialPreviewStyle m_Style;
		
		void RenderThumbnail(Ref<Texture2D> thumbnail);
		void RenderInfo(const std::string& materialName, const std::string& assetPath, bool hasOverrides);
		MaterialPreviewResult RenderActions(bool hasOverrides);
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	MaterialPreviewResult RenderMaterialPreview(const std::string& id,
												const std::string& materialName,
												Ref<Texture2D> thumbnail,
												bool hasOverrides = false,
												const std::string& assetPath = "",
												const MaterialPreviewStyle& style = MaterialPreviewStyle());

} // namespace Lunex::UI
