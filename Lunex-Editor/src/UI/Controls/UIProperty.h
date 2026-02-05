/**
 * @file UIProperty.h
 * @brief Property Grid UI Components
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// PROPERTY COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Vec3 control with colored XYZ buttons
	 * @return true if any value changed
	 */
	bool Vec3Control(const std::string& label,
					 glm::vec3& values,
					 float resetValue = 0.0f,
					 float columnWidth = SpacingValues::PropertyLabelWidth);
	
	/**
	 * @brief Vec2 control
	 */
	bool Vec2Control(const std::string& label,
					 glm::vec2& values,
					 float resetValue = 0.0f,
					 float columnWidth = SpacingValues::PropertyLabelWidth);
	
	/**
	 * @brief Property row with label and control
	 */
	void BeginPropertyRow(const std::string& label, const char* tooltip = nullptr);
	void EndPropertyRow();
	
	/**
	 * @brief Shorthand property controls that include label
	 */
	bool PropertyFloat(const std::string& label, float& value,
					   float speed = 0.1f, float min = 0.0f, float max = 0.0f,
					   const char* tooltip = nullptr);
	
	bool PropertySlider(const std::string& label, float& value,
						float min, float max, const char* format = "%.2f",
						const char* tooltip = nullptr);
	
	bool PropertyColor(const std::string& label, Color3& color,
					   const char* tooltip = nullptr);
	
	bool PropertyColor4(const std::string& label, Color& color,
						const char* tooltip = nullptr);
	
	bool PropertyCheckbox(const std::string& label, bool& value,
						  const char* tooltip = nullptr);
	
	bool PropertyDropdown(const std::string& label, int& selectedIndex,
						  const std::vector<std::string>& options,
						  const char* tooltip = nullptr);
	
	bool PropertyDropdown(const std::string& label, int& selectedIndex,
						  const char* const* options, int optionCount,
						  const char* tooltip = nullptr);
	
	bool PropertyVec2(const std::string& label, glm::vec2& value,
					  float speed = 0.01f, const char* tooltip = nullptr);
	
	bool PropertyVec3(const std::string& label, glm::vec3& value,
					  float speed = 0.01f, const char* tooltip = nullptr);

} // namespace Lunex::UI
