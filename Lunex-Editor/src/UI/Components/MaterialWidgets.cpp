/**
 * @file MaterialWidgets.cpp
 * @brief Lunex UI Framework - Material Editor Widgets Implementation
 */

#include "stpch.h"
#include "MaterialWidgets.h"
#include "../Controls/UIText.h"
#include "../Controls/UIImage.h"
#include "../../Panels/ContentBrowserPanel.h"

#include <cstdarg>

namespace Lunex::UI {

	// ============================================================================
	// COLLAPSIBLE SECTION
	// ============================================================================

	bool CollapsibleSection(const char* label, bool& isOpen,
							const Color* accentColor,
							const CollapsibleSectionStyle& style)
	{
		ImGui::PushID(label);

		float fullWidth = ImGui::GetContentRegionAvail().x;
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		float headerH = style.Height;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 headerMin = cursorPos;
		ImVec2 headerMax = ImVec2(cursorPos.x + fullWidth, cursorPos.y + headerH);

		bool hovered = ImGui::IsMouseHoveringRect(headerMin, headerMax);
		Color bgColor = hovered ? style.BgHovered : style.BgNormal;
		drawList->AddRectFilled(headerMin, headerMax, bgColor.ToImU32(), 0.0f);

		// Bottom border
		drawList->AddLine(
			ImVec2(headerMin.x, headerMax.y),
			ImVec2(headerMax.x, headerMax.y),
			style.BorderColor.ToImU32());

		// Accent bar on the left
		if (accentColor) {
			drawList->AddRectFilled(
				headerMin,
				ImVec2(headerMin.x + style.AccentWidth, headerMax.y),
				accentColor->ToImU32());
		}

		// Arrow indicator
		float arrowX = headerMin.x + (accentColor ? 12.0f : 8.0f);
		float arrowY = headerMin.y + (headerH - ImGui::GetTextLineHeight()) * 0.5f;
		ImGui::SetCursorScreenPos(ImVec2(arrowX, arrowY));
		{
			ScopedColor arrowColor(ImGuiCol_Text, style.ArrowColor);
			ImGui::TextUnformatted(isOpen ? "v" : ">");
		}

		// Label text
		float textX = arrowX + 18.0f;
		ImGui::SetCursorScreenPos(ImVec2(textX, arrowY));
		{
			ScopedColor headerTextColor(ImGuiCol_Text, style.TextColor);
			ImGui::TextUnformatted(label);
		}

		// Invisible button for click handling
		ImGui::SetCursorScreenPos(headerMin);
		if (ImGui::InvisibleButton("##Toggle", ImVec2(fullWidth, headerH))) {
			isOpen = !isOpen;
		}

		ImGui::PopID();
		return isOpen;
	}

	// ============================================================================
	// MATERIAL TEXTURE SLOT
	// ============================================================================

	void MaterialTextureSlot(const std::string& label,
							 Ref<Texture2D> texture,
							 const std::string& path,
							 std::function<void(Ref<Texture2D>)> onTextureSet,
							 std::function<void()> onTextureClear,
							 const TextureSlotStyle& style)
	{
		ScopedID slotID(label);

		float slotWidth = ImGui::GetContentRegionAvail().x;
		ImVec2 startPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 slotMin = startPos;
		ImVec2 slotMax = ImVec2(startPos.x + slotWidth, startPos.y + style.Height);

		bool slotHovered = ImGui::IsMouseHoveringRect(slotMin, slotMax);
		Color slotBg = slotHovered ? style.BgHovered : style.BgNormal;
		drawList->AddRectFilled(slotMin, slotMax, slotBg.ToImU32(), style.Rounding);
		drawList->AddRect(slotMin, slotMax, style.BorderColor.ToImU32(), style.Rounding);

		float pad = style.Padding;

		if (texture && texture->IsLoaded()) {
			// Thumbnail
			float thumbSize = style.ThumbnailSize;
			ImVec2 thumbMin(slotMin.x + pad, slotMin.y + (style.Height - thumbSize) * 0.5f);
			ImVec2 thumbMax(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
			drawList->AddImageRounded(
				(ImTextureID)(intptr_t)texture->GetRendererID(),
				thumbMin, thumbMax,
				ImVec2(0, 1), ImVec2(1, 0),
				IM_COL32(255, 255, 255, 255),
				3.0f);

			// Filename
			float textX = slotMin.x + pad + thumbSize + 8.0f;
			float textY = slotMin.y + (style.Height - ImGui::GetTextLineHeight() * 2.5f) * 0.5f;
			ImGui::SetCursorScreenPos(ImVec2(textX, textY));
			{
				ScopedColor tc(ImGuiCol_Text, style.TextName);
				std::filesystem::path texPath(path);
				ImGui::TextUnformatted(texPath.filename().string().c_str());
			}

			// Dimensions
			ImGui::SetCursorScreenPos(ImVec2(textX, textY + ImGui::GetTextLineHeight() + 2.0f));
			{
				ScopedColor tc(ImGuiCol_Text, style.TextInfo);
				ImGui::Text("%dx%d", texture->GetWidth(), texture->GetHeight());
			}

			// Remove button
			float btnW = 16.0f;
			float btnH = 16.0f;
			ImGui::SetCursorScreenPos(ImVec2(slotMax.x - btnW - pad - 2.0f, slotMin.y + (style.Height - btnH) * 0.5f));
			{
				ScopedColor btnColors({
					{ImGuiCol_Button, style.RemoveBg},
					{ImGuiCol_ButtonHovered, style.RemoveHover},
					{ImGuiCol_ButtonActive, style.RemoveBg.Darker(0.1f)}
				});
				ScopedStyle btnStyle(ImGuiStyleVar_FrameRounding, 3.0f);
				if (ImGui::SmallButton("X")) {
					onTextureClear();
				}
			}
		}
		else {
			// Empty state
			float textY = slotMin.y + (style.Height - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::SetCursorScreenPos(ImVec2(slotMin.x + pad + 4.0f, textY));
			{
				ScopedColor tc(ImGuiCol_Text, style.TextInfo);
				ImGui::Text("%s - Drop texture here", label.c_str());
			}

			if (slotHovered) {
				drawList->AddRect(
					ImVec2(slotMin.x + 2, slotMin.y + 2),
					ImVec2(slotMax.x - 2, slotMax.y - 2),
					style.AccentColor.ToImU32(),
					3.0f, 0, 1.0f);
			}
		}

		// Reserve space and handle drag & drop
		ImGui::SetCursorScreenPos(slotMin);
		ImGui::InvisibleButton("##DropTarget", ImVec2(slotWidth, style.Height));

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEM)) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;

				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
					ext == ".tga" || ext == ".bmp" || ext == ".hdr")
				{
					auto newTexture = Texture2D::Create(data->FilePath);
					if (newTexture && newTexture->IsLoaded()) {
						onTextureSet(newTexture);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SetCursorScreenPos(ImVec2(slotMin.x, slotMax.y + 2.0f));
	}

	// ============================================================================
	// MATERIAL NAME BAR
	// ============================================================================

	bool MaterialNameBar(const std::string& name, bool hasUnsavedChanges,
						 const NameBarStyle& style)
	{
		bool saveClicked = false;

		float fullWidth = ImGui::GetContentRegionAvail().x;
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float h = style.Height;

		drawList->AddRectFilled(
			cursorPos,
			ImVec2(cursorPos.x + fullWidth, cursorPos.y + h),
			style.BgColor.ToImU32());

		drawList->AddLine(
			ImVec2(cursorPos.x, cursorPos.y + h),
			ImVec2(cursorPos.x + fullWidth, cursorPos.y + h),
			style.BorderColor.ToImU32());

		// Accent bar
		drawList->AddRectFilled(
			cursorPos,
			ImVec2(cursorPos.x + style.AccentWidth, cursorPos.y + h),
			style.AccentColor.ToImU32());

		// Name text
		float textY = cursorPos.y + (h - ImGui::GetTextLineHeight()) * 0.5f;
		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 12.0f, textY));
		{
			ScopedColor tc(ImGuiCol_Text, style.TextColor);
			ImGui::TextUnformatted(name.c_str());
		}

		// Save button
		if (hasUnsavedChanges) {
			float saveBtnW = 50.0f;
			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + fullWidth - saveBtnW - 8.0f, cursorPos.y + 4.0f));
			ScopedColor btnColors({
				{ImGuiCol_Button, style.AccentColor.Darker(0.1f)},
				{ImGuiCol_ButtonHovered, style.AccentColor},
				{ImGuiCol_ButtonActive, style.AccentColor.Darker(0.2f)}
			});
			if (ImGui::SmallButton("Save")) {
				saveClicked = true;
			}
		}

		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + h + 1.0f));
		return saveClicked;
	}

	// ============================================================================
	// STATUS BADGE
	// ============================================================================

	void StatusBadge(const std::string& text, const Color& color) {
		ScopedColor tc(ImGuiCol_Text, color);
		ImGui::TextUnformatted(text.c_str());
	}

	// ============================================================================
	// SECTION CONTENT AREA
	// ============================================================================

	bool BeginSectionContent(const std::string& id, const Color& bgColor) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor.ToImVec4());
		bool visible = ImGui::BeginChild(id.c_str(), ImVec2(0, 0),
										 ImGuiChildFlags_AutoResizeY,
										 ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		return visible;
	}

	void EndSectionContent() {
		ImGui::EndChild();
	}

	// ============================================================================
	// INFO ROW
	// ============================================================================

	void InfoRow(const char* label, const char* fmt, ...) {
		{
			ScopedColor tc(ImGuiCol_Text, Color(0.70f, 0.70f, 0.75f, 1.0f));
			ImGui::TextUnformatted(label);
		}
		ImGui::SameLine(130.0f);

		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}

	// ============================================================================
	// DRAG DROP TARGET HELPER
	// ============================================================================

	std::string AcceptTextureDragDrop() {
		std::string result;
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEM)) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
					ext == ".tga" || ext == ".bmp" || ext == ".hdr")
				{
					result = data->FilePath;
				}
			}
			ImGui::EndDragDropTarget();
		}
		return result;
	}

} // namespace Lunex::UI
