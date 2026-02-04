/**
 * @file MaterialPreview.cpp
 * @brief Material Preview Component Implementation
 */

#include "stpch.h"
#include "MaterialPreview.h"
#include <filesystem>

namespace Lunex::UI {

	MaterialPreviewResult MaterialPreview::Render(const std::string& id, const std::string& materialName,
												  Ref<Texture2D> thumbnail, bool hasOverrides,
												  const std::string& assetPath) {
		MaterialPreviewResult result;
		
		ScopedID scopedID(id);
		
		CardStyle cardStyle;
		cardStyle.backgroundColor = Colors::BgDark();
		cardStyle.padding = SpacingValues::MD;
		
		if (BeginCard(id + "_card", Size(-1, 150), cardStyle)) {
			ImGui::BeginGroup();
			RenderThumbnail(thumbnail);
			ImGui::EndGroup();
			
			ImGui::SameLine();
			
			ImGui::BeginGroup();
			RenderInfo(materialName, assetPath, hasOverrides);
			ImGui::EndGroup();
			
			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::SM);
			
			result = RenderActions(hasOverrides);
		}
		EndCard();
		
		return result;
	}
	
	void MaterialPreview::RenderThumbnail(Ref<Texture2D> thumbnail) {
		if (thumbnail) {
			Image(thumbnail, m_Style.size, true);
			
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Material Preview\nClick 'Edit Material' to modify");
			}
		} else {
			ScopedColor btnColor(ImGuiCol_Button, Colors::BgMedium());
			ImGui::Button("##preview", ToImVec2(m_Style.size));
		}
	}
	
	void MaterialPreview::RenderInfo(const std::string& materialName, const std::string& assetPath, bool hasOverrides) {
		TextStyled("?? " + materialName, TextVariant::Primary);
		
		if (!assetPath.empty()) {
			std::filesystem::path matPath(assetPath);
			TextStyled("?? " + matPath.filename().string(), TextVariant::Muted);
		} else {
			TextStyled("?? Default Material", TextVariant::Muted);
		}
		
		AddSpacing(SpacingValues::SM);
		
		if (hasOverrides) {
			TextStyled("?? Has local overrides", TextVariant::Warning);
		} else {
			TextStyled("? Using base asset", TextVariant::Success);
		}
	}
	
	MaterialPreviewResult MaterialPreview::RenderActions(bool hasOverrides) {
		MaterialPreviewResult result;
		
		ImGui::BeginGroup();
		
		if (Button("??? Edit Material", ButtonVariant::Primary, ButtonSize::Small, Size(140, 0))) {
			result.editClicked = true;
		}
		
		SameLine();
		
		if (hasOverrides) {
			if (Button("?? Reset Overrides", ButtonVariant::Warning, ButtonSize::Small, Size(140, 0))) {
				result.resetClicked = true;
			}
		}
		
		ImGui::EndGroup();
		
		return result;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	MaterialPreviewResult RenderMaterialPreview(const std::string& id, const std::string& materialName,
												Ref<Texture2D> thumbnail, bool hasOverrides,
												const std::string& assetPath,
												const MaterialPreviewStyle& style) {
		MaterialPreview preview;
		preview.SetStyle(style);
		return preview.Render(id, materialName, thumbnail, hasOverrides, assetPath);
	}

} // namespace Lunex::UI
