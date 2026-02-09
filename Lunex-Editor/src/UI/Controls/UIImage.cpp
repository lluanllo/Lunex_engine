/**
 * @file UIImage.cpp
 * @brief Image UI Components Implementation
 */

#include "stpch.h"
#include "UIImage.h"
#include "UIText.h"

namespace Lunex::UI {

	void Image(Ref<Texture2D> texture, const Size& size, bool flipY, const Color& tint) {
		if (!texture || texture->GetRendererID() == 0) return;
		Image(texture->GetRendererID(), size, flipY, tint);
	}
	
	void Image(uint32_t textureID, const Size& size, bool flipY, const Color& tint) {
		if (textureID == 0) return;
		
		ImVec2 uv0 = flipY ? ImVec2(0, 1) : ImVec2(0, 0);
		ImVec2 uv1 = flipY ? ImVec2(1, 0) : ImVec2(1, 1);
		
		ImGui::Image(
			(ImTextureID)(intptr_t)textureID,
			ToImVec2(size),
			uv0, uv1,
			ToImVec4(tint),
			ImVec4(0, 0, 0, 0)
		);
	}
	
	bool ImageButton(const std::string& id, Ref<Texture2D> texture, const Size& size, bool flipY, const char* tooltip) {
		if (!texture || texture->GetRendererID() == 0) return false;
		
		ImVec2 uv0 = flipY ? ImVec2(0, 1) : ImVec2(0, 0);
		ImVec2 uv1 = flipY ? ImVec2(1, 0) : ImVec2(1, 1);
		
		bool clicked = ImGui::ImageButton(
			id.c_str(),
			(ImTextureID)(intptr_t)texture->GetRendererID(),
			ToImVec2(size),
			uv0, uv1
		);
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return clicked;
	}
	
	TextureSlotResult TextureSlot(const std::string& label, const char* icon, Ref<Texture2D> currentTexture, const std::string& currentPath) {
		TextureSlotResult result;
		
		ScopedID scopedID(label);
		ScopedColor bgColor(ImGuiCol_ChildBg, Colors::BgDark());
		
		ImGui::BeginChild(("##Tex" + label).c_str(), ImVec2(-1, 80), true);
		
		// Header with icon and label
		ImGui::Text("%s %s", icon, label.c_str());
		
		// Remove button if texture exists
		if (currentTexture) {
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
			ScopedColor dangerColor(ImGuiCol_Button, Colors::Danger());
			if (ImGui::Button("Remove", ImVec2(60, 0))) {
				result.removeClicked = true;
			}
		}
		
		ImGui::Separator();
		
		// Thumbnail or drop zone
		if (currentTexture && currentTexture->IsLoaded() && currentTexture->GetRendererID() != 0) {
			ImGui::Image(
				(ImTextureID)(intptr_t)currentTexture->GetRendererID(),
				ImVec2(50, 50),
				ImVec2(0, 1), ImVec2(1, 0)
			);
			ImGui::SameLine();
			
			std::filesystem::path texPath(currentPath);
			TextStyled(texPath.filename().string(), TextVariant::Muted);
		} else {
			TextStyled("Drop texture here", TextVariant::Muted);
		}
		
		// Drag and drop
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEM)) {
				const char* droppedPath = (const char*)payload->Data;
				result.droppedPath = droppedPath;
				result.textureChanged = true;
			}
			ImGui::EndDragDropTarget();
		}
		
		ImGui::EndChild();
		
		return result;
	}
	
	void Thumbnail(Ref<Texture2D> texture, const Size& size, bool selected, bool hovered) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
		
		// Background
		drawList->AddRectFilled(pos, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(Colors::BgMedium())), 4.0f);
		
		// Image
		if (texture && texture->GetRendererID() != 0) {
			drawList->AddImageRounded(
				(ImTextureID)(intptr_t)texture->GetRendererID(),
				pos, max,
				ImVec2(0, 1), ImVec2(1, 0),
				IM_COL32(255, 255, 255, 255),
				4.0f
			);
		}
		
		// Selection/hover effects
		if (selected) {
			drawList->AddRect(pos, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(Colors::SelectedBorder())), 4.0f, 0, 2.5f);
			Color fill = Colors::Selected();
			fill.a = 0.15f;
			drawList->AddRectFilled(pos, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(fill)), 4.0f);
		} else if (hovered) {
			drawList->AddRect(pos, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(Colors::BorderLight())), 4.0f, 0, 2.0f);
		}
		
		// Advance cursor
		ImGui::Dummy(ToImVec2(size));
	}
	
	bool ColorPreviewButton(const std::string& id, const Color& color, const Size& size) {
		ScopedID scopedID(id);
		ScopedColor btnColor(ImGuiCol_Button, color);
		return ImGui::Button("##colorpreview", ToImVec2(size));
	}

} // namespace Lunex::UI
