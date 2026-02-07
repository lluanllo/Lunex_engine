/**
 * @file UIInput.cpp
 * @brief Input UI Components Implementation
 */

#include "stpch.h"
#include "UIInput.h"

namespace Lunex::UI {

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
			return ImGui::ColorEdit4("##color", color.data(), flags);
		}
		return ImGui::ColorEdit3("##color", color.data(), flags);
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
	
	bool ComboBox(const std::string& id, int& selectedIndex, const char* const* items, int itemCount, const char* tooltip) {
		if (itemCount <= 0) return false;
		
		ScopedID scopedID(id);
		ImGui::SetNextItemWidth(-1);
		
		const char* currentItem = items[selectedIndex];
		bool changed = false;
		
		if (ImGui::BeginCombo("##combo", currentItem)) {
			for (int i = 0; i < itemCount; i++) {
				bool isSelected = (selectedIndex == i);
				if (ImGui::Selectable(items[i], isSelected)) {
					selectedIndex = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
		
		return changed;
	}

} // namespace Lunex::UI
