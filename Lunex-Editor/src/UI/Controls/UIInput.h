/**
 * @file UIInput.h
 * @brief Input UI Components
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// INPUT COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Text input field
	 * @return true if value changed
	 */
	bool InputText(const std::string& id,
				   std::string& value,
				   const char* placeholder = nullptr,
				   InputVariant variant = InputVariant::Default);
	
	bool InputText(const std::string& id,
				   char* buffer,
				   size_t bufferSize,
				   const char* placeholder = nullptr,
				   InputVariant variant = InputVariant::Default);
	
	/**
	 * @brief Multiline text input
	 */
	bool InputTextMultiline(const std::string& id,
							std::string& value,
							const Size& size = Size(0, 100));
	
	/**
	 * @brief Numeric input with drag functionality
	 */
	bool InputFloat(const std::string& id,
					float& value,
					float speed = 0.1f,
					float min = 0.0f,
					float max = 0.0f,
					const char* format = "%.2f");
	
	bool InputFloat2(const std::string& id,
					 glm::vec2& value,
					 float speed = 0.1f,
					 const char* format = "%.2f");
	
	bool InputFloat3(const std::string& id,
					 glm::vec3& value,
					 float speed = 0.1f,
					 const char* format = "%.2f");
	
	bool InputInt(const std::string& id,
				  int& value,
				  int step = 1,
				  int min = 0,
				  int max = 0);
	
	/**
	 * @brief Slider input
	 */
	bool Slider(const std::string& id,
				float& value,
				float min,
				float max,
				const char* format = "%.2f");
	
	bool SliderInt(const std::string& id,
				   int& value,
				   int min,
				   int max);
	
	/**
	 * @brief Checkbox
	 */
	bool Checkbox(const std::string& label, bool& value, const char* tooltip = nullptr);
	
	/**
	 * @brief Color picker
	 */
	bool ColorPicker(const std::string& id, Color& color, bool showAlpha = true);
	bool ColorPicker3(const std::string& id, Color3& color);
	
	/**
	 * @brief Dropdown/Combo box
	 */
	bool Dropdown(const std::string& id,
				  int& selectedIndex,
				  const std::vector<std::string>& options);
	
	bool Dropdown(const std::string& id,
				  int& selectedIndex,
				  const char* const* options,
				  int optionCount);
	
	/**
	 * @brief Combo box with tooltip
	 */
	bool ComboBox(const std::string& id,
				  int& selectedIndex,
				  const char* const* items,
				  int itemCount,
				  const char* tooltip = nullptr);

} // namespace Lunex::UI
