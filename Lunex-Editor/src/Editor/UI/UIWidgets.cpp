#include "stpch.h"
#include "UIWidgets.h"
#include "UIStyles.h"

namespace Lunex::UI {

	// =========================================
	// PROPERTY GRID HELPERS
	// =========================================

	void Widgets::BeginPropertyGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	}

	void Widgets::EndPropertyGrid() {
		ImGui::PopStyleVar(2);
	}

	void Widgets::PropertyLabel(const char* label, const char* tooltip) {
		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_SUBHEADER);
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
	}

	void Widgets::SectionHeader(const char* icon, const char* title) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_HEADER);
		ImGui::Text("%s  %s", icon, title);
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	void Widgets::SectionSeparator() {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	// =========================================
	// PROPERTY CONTROLS
	// =========================================

	bool Widgets::PropertySlider(const char* label, float* value, float min, float max,
		const char* format, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Style::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, Style::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Style::COLOR_ACCENT_HOVER);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::SliderFloat(("##" + std::string(label)).c_str(), value, min, max, format);
		ImGui::PopStyleColor(3);
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyDrag(const char* label, float* value, float speed, float min, float max,
		const char* format, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Style::COLOR_ACCENT);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat(("##" + std::string(label)).c_str(), value, speed, min, max, format);
		ImGui::PopStyleColor();
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyDragInt(const char* label, int* value, float speed, int min, int max,
		const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Style::COLOR_ACCENT);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragInt(("##" + std::string(label)).c_str(), value, speed, min, max);
		ImGui::PopStyleColor();
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyColor(const char* label, glm::vec3& color, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit3(("##" + std::string(label)).c_str(), 
			glm::value_ptr(color), ImGuiColorEditFlags_NoLabel);
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyColor4(const char* label, glm::vec4& color, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit4(("##" + std::string(label)).c_str(), 
			glm::value_ptr(color), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyCheckbox(const char* label, bool* value, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyText(const char* label, std::string& value, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();

		char buffer[256];
		strncpy_s(buffer, sizeof(buffer), value.c_str(), _TRUNCATE);
		
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::InputText(("##" + std::string(label)).c_str(), buffer, sizeof(buffer));
		if (changed) {
			value = buffer;
		}
		ImGui::Columns(1);
		return changed;
	}

	bool Widgets::PropertyCombo(const char* label, int* currentIndex, const char* const* items,
		int itemCount, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, Style::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		
		bool changed = false;
		if (ImGui::BeginCombo(("##" + std::string(label)).c_str(), items[*currentIndex])) {
			for (int i = 0; i < itemCount; i++) {
				bool isSelected = (*currentIndex == i);
				if (ImGui::Selectable(items[i], isSelected)) {
					*currentIndex = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::Columns(1);
		return changed;
	}

	// =========================================
	// VECTOR CONTROLS
	// =========================================

	void Widgets::DrawVec3Control(const std::string& label, glm::vec3& values,
		float resetValue, float columnWidth) {
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);

		PropertyLabel(label.c_str());

		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });

		// X Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.70f, 0.20f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.80f, 0.30f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.60f, 0.15f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", ImVec2{ 25, 25 }))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.25f, 0.15f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.30f, 0.18f, 0.18f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.70f, 0.20f, 0.20f, 0.50f });
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Y Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.70f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.80f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.60f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", ImVec2{ 25, 25 }))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.15f, 0.25f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.18f, 0.30f, 0.18f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.70f, 0.20f, 0.20f, 0.50f });
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Z Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.40f, 0.90f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.50f, 1.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.35f, 0.80f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", ImVec2{ 25, 25 }))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.15f, 0.18f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.18f, 0.22f, 0.35f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.20f, 0.40f, 0.90f, 0.50f });
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	bool Widgets::DrawVec2Control(const char* label, glm::vec2& values, float resetValue, float columnWidth) {
		bool changed = false;

		ImGui::PushID(label);
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		PropertyLabel(label);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Style::COLOR_ACCENT);
		ImGui::SetNextItemWidth(-1);
		changed = ImGui::DragFloat2(("##" + std::string(label)).c_str(), glm::value_ptr(values), 0.01f);
		ImGui::PopStyleColor();
		ImGui::Columns(1);
		ImGui::PopID();

		return changed;
	}

	// =========================================
	// BUTTONS
	// =========================================

	bool Widgets::PrimaryButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::COLOR_ACCENT_HOVER);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::COLOR_ACCENT_ACTIVE);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	bool Widgets::DangerButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_DANGER);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	bool Widgets::SuccessButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_SUCCESS);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	bool Widgets::SecondaryButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_BG_MEDIUM);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::COLOR_BG_LIGHT);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::COLOR_BG_DARK);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	bool Widgets::IconButton(const char* icon, const char* tooltip, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_BG_MEDIUM);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::COLOR_BG_LIGHT);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::COLOR_BG_DARK);
		bool clicked = ImGui::Button(icon, size);
		ImGui::PopStyleColor(3);
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		return clicked;
	}

	// =========================================
	// ASSET DROP ZONES
	// =========================================

	bool Widgets::AssetDropZone(const char* label, const char* acceptedTypes, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_BG_MEDIUM);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::COLOR_BG_LIGHT);
		ImGui::PushStyleColor(ImGuiCol_Border, Style::COLOR_ACCENT);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
		
		std::string buttonLabel = std::string("?? ") + label + "\n" + acceptedTypes;
		ImGui::Button(buttonLabel.c_str(), size);
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		return ImGui::BeginDragDropTarget();
	}

	bool Widgets::TextureDropZone(const char* label, uint32_t textureID, const ImVec2& size) {
		if (textureID != 0) {
			ImGui::Image((ImTextureID)(intptr_t)textureID, size, ImVec2(0, 1), ImVec2(1, 0));
			return ImGui::BeginDragDropTarget();
		}
		return AssetDropZone(label, "(.png, .jpg, .bmp, .tga, .hdr)", size);
	}

	// =========================================
	// CARDS & CONTAINERS
	// =========================================

	bool Widgets::BeginCard(const char* id, const ImVec2& size, bool border, const ImVec4& borderColor) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Style::COLOR_BG_DARK);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Style::CARD_ROUNDING);
		
		bool result = ImGui::BeginChild(id, size, true, 
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		if (border && borderColor.w > 0) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 min = ImGui::GetWindowPos();
			ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
			drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(borderColor), 
				Style::CARD_ROUNDING, 0, 2.0f);
		}

		return result;
	}

	void Widgets::EndCard() {
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	bool Widgets::BeginComponentHeader(const char* label, bool* removeComponent, bool canRemove) {
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen 
			| ImGuiTreeNodeFlags_Framed 
			| ImGuiTreeNodeFlags_SpanAvailWidth 
			| ImGuiTreeNodeFlags_AllowOverlap 
			| ImGuiTreeNodeFlags_FramePadding;

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 6, 6 });
		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;

		ImGui::PushStyleColor(ImGuiCol_Header, Style::COLOR_BG_MEDIUM);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.32f, 1.0f));

		bool open = ImGui::TreeNodeEx(label, treeNodeFlags);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();

		// Remove button
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);

		if (!canRemove) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.42f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.37f, 1.0f));

		if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight })) {
			if (canRemove) {
				ImGui::OpenPopup("ComponentSettings");
			}
		}

		ImGui::PopStyleColor(3);

		if (!canRemove) {
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("This component cannot be removed independently");
			}
		}

		if (removeComponent && canRemove && ImGui::BeginPopup("ComponentSettings")) {
			if (ImGui::MenuItem("Remove component")) {
				*removeComponent = true;
			}
			ImGui::EndPopup();
		}

		return open;
	}

	void Widgets::EndComponentHeader(bool open) {
		if (open) {
			ImGui::TreePop();
		}
	}

	// =========================================
	// STATUS INDICATORS
	// =========================================

	void Widgets::StatusBadge(const char* text, const ImVec4& color) {
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::Text("%s", text);
		ImGui::PopStyleColor();
	}

	void Widgets::ProgressBar(float progress, const ImVec2& size, const char* overlay) {
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Style::COLOR_ACCENT);
		ImGui::ProgressBar(progress, size, overlay);
		ImGui::PopStyleColor();
	}

	// =========================================
	// UTILITY
	// =========================================

	void Widgets::CenteredText(const char* text) {
		float windowWidth = ImGui::GetContentRegionAvail().x;
		float textWidth = ImGui::CalcTextSize(text).x;
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text("%s", text);
	}

	void Widgets::HelpMarker(const char* desc) {
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	void Widgets::DrawShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max,
		float rounding, float offset, float alpha) {
		ImVec2 shadowMin = ImVec2(min.x + offset, min.y + offset);
		ImVec2 shadowMax = ImVec2(max.x + offset, max.y + offset);
		ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
		drawList->AddRectFilled(shadowMin, shadowMax, shadowColor, rounding);
	}

	std::string Widgets::TruncateText(const std::string& text, int maxChars) {
		if ((int)text.length() <= maxChars) {
			return text;
		}
		return text.substr(0, maxChars - 2) + "..";
	}

} // namespace Lunex::UI
