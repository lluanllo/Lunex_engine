/**
 * @file UIProperty.cpp
 * @brief Property Grid UI Components Implementation
 */

#include "stpch.h"
#include "UIProperty.h"
#include "UIText.h"
#include "UIInput.h"

namespace Lunex::UI {

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
				{ImGuiCol_ButtonActive, Color(0.75f, 0.15f, 0.15f, 1.0f)}
			});
			
			if (ImGui::Button("X", ImVec2(25, 25))) {
				values.x = resetValue;
				changed = true;
			}
		}
		
		ImGui::SameLine();
		
		{
			ScopedColor frameColors({
				{ImGuiCol_FrameBg, Color(0.18f, 0.10f, 0.10f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.25f, 0.12f, 0.12f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.89f, 0.22f, 0.21f, 0.40f)}
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
				{ImGuiCol_FrameBg, Color(0.10f, 0.18f, 0.10f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.12f, 0.25f, 0.12f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.27f, 0.75f, 0.27f, 0.40f)}
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
				{ImGuiCol_FrameBg, Color(0.10f, 0.12f, 0.22f, 1.0f)},
				{ImGuiCol_FrameBgHovered, Color(0.12f, 0.16f, 0.30f, 1.0f)},
				{ImGuiCol_FrameBgActive, Color(0.22f, 0.46f, 0.93f, 0.40f)}
			});
			
			if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
				changed = true;
			}
		}
		
		ImGui::PopItemWidth();
		ImGui::Columns(1);
		
		return changed;
	}
	
	bool Vec2Control(const std::string& label, glm::vec2& values, float resetValue, float columnWidth) {
		ScopedID scopedID(label);
		
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		
		Label(label);
		
		ImGui::NextColumn();
		
		ScopedColor colors({
			{ImGuiCol_FrameBg, Colors::BgMedium()},
			{ImGuiCol_FrameBgHovered, Colors::BgHover()},
			{ImGuiCol_FrameBgActive, Colors::Primary()}
		});
		
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat2("##vec2", glm::value_ptr(values), 0.01f);
		
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
		bool changed = ImGui::ColorEdit4(("##" + label).c_str(), color.data(), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
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
	
	bool PropertyVec2(const std::string& label, glm::vec2& value, float speed, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ScopedColor colors({
			{ImGuiCol_FrameBgActive, Colors::Primary()}
		});
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat2(("##" + label).c_str(), glm::value_ptr(value), speed);
		EndPropertyRow();
		return changed;
	}
	
	bool PropertyVec3(const std::string& label, glm::vec3& value, float speed, const char* tooltip) {
		BeginPropertyRow(label, tooltip);
		ScopedColor colors({
			{ImGuiCol_FrameBgActive, Colors::Primary()}
		});
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat3(("##" + label).c_str(), glm::value_ptr(value), speed);
		EndPropertyRow();
		return changed;
	}

} // namespace Lunex::UI
