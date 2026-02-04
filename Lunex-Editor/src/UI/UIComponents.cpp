/**
 * @file UIComponents.cpp
 * @brief Implementation of UI Components
 */

#include "stpch.h"
#include "UIComponents.h"

namespace Lunex::UI {

	// ============================================================================
	// TEXT COMPONENTS
	// ============================================================================
	
	void TextStyled(const std::string& text, TextVariant variant) {
		Color color;
		switch (variant) {
			case TextVariant::Primary:   color = Colors::TextPrimary(); break;
			case TextVariant::Secondary: color = Colors::TextSecondary(); break;
			case TextVariant::Muted:     color = Colors::TextMuted(); break;
			case TextVariant::Success:   color = Colors::Success(); break;
			case TextVariant::Warning:   color = Colors::Warning(); break;
			case TextVariant::Danger:    color = Colors::Danger(); break;
			default:                     color = Colors::TextPrimary(); break;
		}
		
		ScopedColor col(ImGuiCol_Text, color);
		ImGui::TextUnformatted(text.c_str());
	}
	
	void Text(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}
	
	void TextWrapped(const std::string& text, TextVariant variant) {
		Color color;
		switch (variant) {
			case TextVariant::Primary:   color = Colors::TextPrimary(); break;
			case TextVariant::Secondary: color = Colors::TextSecondary(); break;
			case TextVariant::Muted:     color = Colors::TextMuted(); break;
			case TextVariant::Success:   color = Colors::Success(); break;
			case TextVariant::Warning:   color = Colors::Warning(); break;
			case TextVariant::Danger:    color = Colors::Danger(); break;
			default:                     color = Colors::TextPrimary(); break;
		}
		
		ScopedColor col(ImGuiCol_Text, color);
		ImGui::TextWrapped("%s", text.c_str());
	}
	
	void Label(const std::string& text, const char* tooltip) {
		ImGui::AlignTextToFramePadding();
		ScopedColor col(ImGuiCol_Text, Colors::TextSecondary());
		ImGui::TextUnformatted(text.c_str());
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
	}
	
	void Heading(const std::string& text, int level) {
		float scale = 1.0f;
		switch (level) {
			case 1: scale = 1.4f; break;
			case 2: scale = 1.2f; break;
			case 3: scale = 1.1f; break;
			default: scale = 1.0f; break;
		}
		
		ScopedColor col(ImGuiCol_Text, Colors::TextPrimary());
		ImGui::SetWindowFontScale(scale);
		ImGui::TextUnformatted(text.c_str());
		ImGui::SetWindowFontScale(1.0f);
	}
	
	void Badge(const std::string& text, const Color& bgColor, const Color& textColor) {
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
		ImVec2 padding(6.0f, 2.0f);
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		ImVec2 min = pos;
		ImVec2 max = ImVec2(pos.x + textSize.x + padding.x * 2, pos.y + textSize.y + padding.y * 2);
		
		drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(bgColor)), 3.0f);
		
		ImGui::SetCursorScreenPos(ImVec2(pos.x + padding.x, pos.y + padding.y));
		ScopedColor col(ImGuiCol_Text, textColor);
		ImGui::TextUnformatted(text.c_str());
		
		ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y + 2.0f));
	}
	
	// ============================================================================
	// BUTTON COMPONENTS
	// ============================================================================
	
	static void ApplyButtonStyle(ButtonVariant variant) {
		switch (variant) {
			case ButtonVariant::Primary:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Primary()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::PrimaryHover()));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Colors::PrimaryActive()));
				break;
			case ButtonVariant::Success:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Success()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.7f, 0.25f, 1.0f));
				break;
			case ButtonVariant::Warning:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Warning()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.15f, 1.0f));
				break;
			case ButtonVariant::Danger:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::Danger()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
				break;
			case ButtonVariant::Ghost:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::BgHover()));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Colors::BgLight()));
				break;
			case ButtonVariant::Outline:
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Color(Colors::Primary().r, Colors::Primary().g, Colors::Primary().b, 0.2f)));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Color(Colors::Primary().r, Colors::Primary().g, Colors::Primary().b, 0.3f)));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(Colors::Primary()));
				break;
			default:
				ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::BgLight()));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::BgHover()));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Colors::BgMedium()));
				break;
		}
	}
	
	static void PopButtonStyle(ButtonVariant variant) {
		if (variant == ButtonVariant::Outline) {
			ImGui::PopStyleColor(4);
			ImGui::PopStyleVar();
		} else {
			ImGui::PopStyleColor(3);
		}
	}
	
	static ImVec2 GetButtonSize(ButtonSize size, const Size& customSize) {
		if (customSize.x > 0 || customSize.y > 0) {
			return ToImVec2(customSize);
		}
		
		switch (size) {
			case ButtonSize::Small:  return ImVec2(0, 22.0f);
			case ButtonSize::Large:  return ImVec2(0, SpacingValues::ButtonHeightLG);
			default:                 return ImVec2(0, SpacingValues::ButtonHeight);
		}
	}
	
	bool Button(const std::string& label, ButtonVariant variant, ButtonSize size, const Size& customSize) {
		ApplyButtonStyle(variant);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		
		ImVec2 btnSize = GetButtonSize(size, customSize);
		bool clicked = ImGui::Button(label.c_str(), btnSize);
		
		PopButtonStyle(variant);
		return clicked;
	}
	
	bool ButtonBlock(const std::string& label, ButtonVariant variant, float height) {
		return Button(label, variant, ButtonSize::Medium, Size(-1, height));
	}
	
	bool IconButton(const std::string& id, Ref<Texture2D> icon, float size, const char* tooltip, const Color& tint) {
		bool clicked = false;
		
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		ScopedColor colors({
			{ImGuiCol_Button, Colors::BgLight()},
			{ImGuiCol_ButtonHovered, Colors::BgHover()},
			{ImGuiCol_ButtonActive, Colors::BgMedium()}
		});
		
		if (icon) {
			clicked = ImGui::ImageButton(id.c_str(),
				(ImTextureID)(intptr_t)icon->GetRendererID(),
				ImVec2(size, size),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0),
				ToImVec4(tint));
		} else {
			clicked = ImGui::Button("##btn", ImVec2(size + 8, size + 8));
		}
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return clicked;
	}
	
	bool IconButton(const std::string& id, Ref<Texture2D> icon, const char* fallbackText, float size, const char* tooltip) {
		if (icon) {
			return IconButton(id, icon, size, tooltip);
		}
		
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::ButtonRounding);
		
		bool clicked = ImGui::Button(fallbackText, ImVec2(size + 8, size + 8));
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return clicked;
	}
	
	bool ButtonWithIcon(const std::string& label, const char* icon, ButtonVariant variant) {
		std::string fullLabel = std::string(icon) + " " + label;
		return Button(fullLabel, variant);
	}
	
	// ============================================================================
	// INPUT COMPONENTS
	// ============================================================================
	
	bool InputText(const std::string& id, std::string& value, const char* placeholder, InputVariant variant) {
		char buffer[1024];
		strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
		
		bool changed = InputText(id, buffer, sizeof(buffer), placeholder, variant);
		
		if (changed) {
			value = buffer;
		}
		
		return changed;
	}
	
	bool InputText(const std::string& id, char* buffer, size_t bufferSize, const char* placeholder, InputVariant variant) {
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::InputRounding);
		
		Color bgColor;
		switch (variant) {
			case InputVariant::Filled:  bgColor = Colors::BgDark(); break;
			case InputVariant::Outline: bgColor = Color(0, 0, 0, 0); break;
			default:                    bgColor = Colors::BgMedium(); break;
		}
		
		ScopedColor colors({
			{ImGuiCol_FrameBg, bgColor},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		ImGui::SetNextItemWidth(-1);
		
		if (placeholder) {
			return ImGui::InputTextWithHint("##input", placeholder, buffer, bufferSize);
		}
		
		return ImGui::InputText("##input", buffer, bufferSize);
	}
	
	bool InputTextMultiline(const std::string& id, std::string& value, const Size& size) {
		char buffer[4096];
		strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
		
		ScopedID scopedID(id);
		ScopedStyle rounding(ImGuiStyleVar_FrameRounding, SpacingValues::InputRounding);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		bool changed = ImGui::InputTextMultiline("##multiline", buffer, sizeof(buffer), ToImVec2(size));
		
		if (changed) {
			value = buffer;
		}
		
		return changed;
	}
	
	bool InputFloat(const std::string& id, float& value, float speed, float min, float max, const char* format) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::DragFloat("##float", &value, speed, min, max, format);
	}
	
	bool InputFloat2(const std::string& id, glm::vec2& value, float speed, const char* format) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::DragFloat2("##float2", glm::value_ptr(value), speed, 0.0f, 0.0f, format);
	}
	
	bool InputFloat3(const std::string& id, glm::vec3& value, float speed, const char* format) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::DragFloat3("##float3", glm::value_ptr(value), speed, 0.0f, 0.0f, format);
	}
	
	bool InputInt(const std::string& id, int& value, int step, int min, int max) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::DragInt("##int", &value, (float)step, min, max);
	}
	
	bool Slider(const std::string& id, float& value, float min, float max, const char* format) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()},
			{ImGuiCol_SliderGrab, Colors::Primary()},
			{ImGuiCol_SliderGrabActive, Colors::PrimaryHover()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::SliderFloat("##slider", &value, min, max, format);
	}
	
	bool SliderInt(const std::string& id, int& value, int min, int max) {
		ScopedID scopedID(id);
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()},
			{ImGuiCol_SliderGrab, Colors::Primary()},
			{ImGuiCol_SliderGrabActive, Colors::PrimaryHover()}
		});
		
		ImGui::SetNextItemWidth(-1);
		return ImGui::SliderInt("##sliderint", &value, min, max);
	}
	
	bool Checkbox(const std::string& label, bool& value, const char* tooltip) {
		bool changed = ImGui::Checkbox(("##" + label).c_str(), &value);
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return changed;
	}
	
	bool ColorPicker(const std::string& id, Color& color, bool showAlpha) {
		ScopedID scopedID(id);
		ImGui::SetNextItemWidth(-1);
		
		ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoLabel;
		if (showAlpha) {
			flags |= ImGuiColorEditFlags_AlphaBar;
			return ImGui::ColorEdit4("##color", glm::value_ptr(color), flags);
		}
		return ImGui::ColorEdit3("##color", glm::value_ptr(color), flags);
	}
	
	bool ColorPicker3(const std::string& id, Color3& color) {
		ScopedID scopedID(id);
		ImGui::SetNextItemWidth(-1);
		return ImGui::ColorEdit3("##color3", glm::value_ptr(color), ImGuiColorEditFlags_NoLabel);
	}
	
	bool Dropdown(const std::string& id, int& selectedIndex, const std::vector<std::string>& options) {
		if (options.empty()) return false;
		
		ScopedID scopedID(id);
		ImGui::SetNextItemWidth(-1);
		
		const char* currentItem = options[selectedIndex].c_str();
		bool changed = false;
		
		if (ImGui::BeginCombo("##combo", currentItem)) {
			for (int i = 0; i < (int)options.size(); i++) {
				bool isSelected = (selectedIndex == i);
				if (ImGui::Selectable(options[i].c_str(), isSelected)) {
					selectedIndex = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		
		return changed;
	}
	
	bool Dropdown(const std::string& id, int& selectedIndex, const char* const* options, int optionCount) {
		if (optionCount <= 0) return false;
		
		ScopedID scopedID(id);
		ImGui::SetNextItemWidth(-1);
		
		const char* currentItem = options[selectedIndex];
		bool changed = false;
		
		if (ImGui::BeginCombo("##combo", currentItem)) {
			for (int i = 0; i < optionCount; i++) {
				bool isSelected = (selectedIndex == i);
				if (ImGui::Selectable(options[i], isSelected)) {
					selectedIndex = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		
		return changed;
	}
	
	// ============================================================================
	// SPECIALIZED CONTROLS
	// ============================================================================
	
	bool Vec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth) {
		ScopedID scopedID(label);
		
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		
		Label(label);
		
		ImGui::NextColumn();
		
		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
		
		bool changed = false;
		
		// X Component
		{
			ScopedColor xColors({
				{ImGuiCol_Button, Colors::AxisX()},
				{ImGuiCol_ButtonHovered, Colors::AxisXHover()},
				{ImGuiCol_ButtonActive, Color(0.60f, 0.15f, 0.15f, 1.0f)}
			});
			
			if (ImGui::Button("X", ImVec2(25, 25))) {
				values.x = resetValue;
				changed = true;
			}
		}
		
		ImGui::SameLine();
		
		{
			ScopedColor frameColors({
				{ImGuiCol_FrameBg, Color(0.25f, 0.15f, 0.15f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.30f, 0.18f, 0.18f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.70f, 0.20f, 0.20f, 0.50f)}
			});
			
			if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
				changed = true;
			}
		}
		
		ImGui::PopItemWidth();
		ImGui::SameLine();
		
		// Y Component
		{
			ScopedColor yColors({
				{ImGuiCol_Button, Colors::AxisY()},
				{ImGuiCol_ButtonHovered, Colors::AxisYHover()},
				{ImGuiCol_ButtonActive, Color(0.15f, 0.60f, 0.15f, 1.0f)}
			});
			
			if (ImGui::Button("Y", ImVec2(25, 25))) {
				values.y = resetValue;
				changed = true;
			}
		}
		
		ImGui::SameLine();
		
		{
			ScopedColor frameColors({
				{ImGuiCol_FrameBg, Color(0.15f, 0.25f, 0.15f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.18f, 0.30f, 0.18f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.20f, 0.70f, 0.20f, 0.50f)}
			});
			
			if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
				changed = true;
			}
		}
		
		ImGui::PopItemWidth();
		ImGui::SameLine();
		
		// Z Component
		{
			ScopedColor zColors({
				{ImGuiCol_Button, Colors::AxisZ()},
				{ImGuiCol_ButtonHovered, Colors::AxisZHover()},
				{ImGuiCol_ButtonActive, Color(0.15f, 0.35f, 0.80f, 1.0f)}
			});
			
			if (ImGui::Button("Z", ImVec2(25, 25))) {
				values.z = resetValue;
				changed = true;
			}
		}
		
		ImGui::SameLine();
		
		{
			ScopedColor frameColors({
				{ImGuiCol_FrameBg, Color(0.15f, 0.18f, 0.30f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.18f, 0.22f, 0.35f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.20f, 0.40f, 0.90f, 0.50f)}
			});
			
			if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
				changed = true;
			}
		}
		
		ImGui::PopItemWidth();
		ImGui::Columns(1);
		
		return changed;
	}
	
	void BeginPropertyRow(const std::string& label, const char* tooltip) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, SpacingValues::PropertyLabelWidth);
		Label(label, tooltip);
		ImGui::NextColumn();
	}
	
	void EndPropertyRow() {
		ImGui::Columns(1);
	}
	
	bool PropertyFloat(const std::string& label, float& value, float speed, float min, float max, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ScopedColor colors({
			{ImGuiCol_FrameBgActive, Colors::Primary()}
		});
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat(("##" + label).c_str(), &value, speed, min, max, "%.2f");
		EndPropertyRow();
		return changed;
	}
	
	bool PropertySlider(const std::string& label, float& value, float min, float max, const char* format, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ScopedColor colors({
			{ImGuiCol_FrameBgActive, Colors::Primary()},
			{ImGuiCol_SliderGrab, Colors::Primary()},
			{ImGuiCol_SliderGrabActive, Colors::PrimaryHover()}
		});
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::SliderFloat(("##" + label).c_str(), &value, min, max, format);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyColor(const std::string& label, Color3& color, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit3(("##" + label).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoLabel);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyColor4(const std::string& label, Color& color, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit4(("##" + label).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyCheckbox(const std::string& label, bool& value, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		bool changed = ImGui::Checkbox(("##" + label).c_str(), &value);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyDropdown(const std::string& label, int& selectedIndex, const std::vector<std::string>& options, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		bool changed = Dropdown(label, selectedIndex, options);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyDropdown(const std::string& label, int& selectedIndex, const char* const* options, int optionCount, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		bool changed = Dropdown(label, selectedIndex, options, optionCount);
		EndPropertyRow();
		return changed;
	}
	
	// ============================================================================
	// IMAGE & TEXTURE COMPONENTS
	// ============================================================================
	
	void Image(Ref<Texture2D> texture, const Size& size, bool flipY, const Color& tint) {
		if (!texture) return;
		Image(texture->GetRendererID(), size, flipY, tint);
	}
	
	void Image(uint32_t textureID, const Size& size, bool flipY, const Color& tint) {
		ImVec2 uv0 = flipY ? ImVec2(0, 1) : ImVec2(0, 0);
		ImVec2 uv1 = flipY ? ImVec2(1, 0) : ImVec2(1, 1);
		
		ImGui::Image(
			(ImTextureID)(intptr_t)textureID,
			ToImVec2(size),
			uv0, uv1,
			ToImVec4(tint),
			ImVec4(0, 0, 0, 0)  // border_col
		);
	}
	
	bool ImageButton(const std::string& id, Ref<Texture2D> texture, const Size& size, bool flipY, const char* tooltip) {
		if (!texture) return false;
		
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
		if (currentTexture && currentTexture->IsLoaded()) {
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
				// Assuming ContentBrowserPayload structure
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
		if (texture) {
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

} // namespace Lunex::UI
