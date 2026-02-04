/**
 * @file UIComponents.h
 * @brief Lunex UI Framework - Core UI Components
 * 
 * Provides reusable UI components that abstract ImGui calls
 * into a cleaner, more declarative API.
 */

#pragma once

#include "UICore.h"
#include <optional>

namespace Lunex::UI {

	// ============================================================================
	// TEXT COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Display text with styling options
	 */
	void TextStyled(const std::string& text, TextVariant variant = TextVariant::Default);
	
	/**
	 * @brief Display text (variadic)
	 */
	void Text(const char* fmt, ...);
	
	void TextWrapped(const std::string& text, TextVariant variant = TextVariant::Default);
	
	void Label(const std::string& text, const char* tooltip = nullptr);
	
	void Heading(const std::string& text, int level = 1);
	
	void Badge(const std::string& text, const Color& bgColor, const Color& textColor = Colors::TextPrimary());
	
	void BulletText(const std::string& text);
	
	// ============================================================================
	// BUTTON COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Standard button with variants
	 * @return true if clicked
	 */
	bool Button(const std::string& label, 
				ButtonVariant variant = ButtonVariant::Default,
				ButtonSize size = ButtonSize::Medium,
				const Size& customSize = Size(0, 0));
	
	/**
	 * @brief Button that fills available width
	 */
	bool ButtonBlock(const std::string& label,
					 ButtonVariant variant = ButtonVariant::Default,
					 float height = SpacingValues::ButtonHeight);
	
	/**
	 * @brief Icon button (square, typically for toolbars)
	 */
	bool IconButton(const std::string& id, 
					Ref<Texture2D> icon,
					float size = SpacingValues::IconLG,
					const char* tooltip = nullptr,
					const Color& tint = Colors::TextPrimary());
	
	/**
	 * @brief Icon button with text fallback
	 */
	bool IconButton(const std::string& id,
					Ref<Texture2D> icon,
					const char* fallbackText,
					float size = SpacingValues::IconLG,
					const char* tooltip = nullptr);
	
	/**
	 * @brief Button with icon and text
	 */
	bool ButtonWithIcon(const std::string& label,
						const char* icon,  // Emoji or text icon
						ButtonVariant variant = ButtonVariant::Default);
	
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
	 * @brief Combo box (same as dropdown but can return the current item string)
	 */
	bool ComboBox(const std::string& id,
				  int& selectedIndex,
				  const char* const* items,
				  int itemCount,
				  const char* tooltip = nullptr);
	
	// ============================================================================
	// SPECIALIZED CONTROLS
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
	 * Helper for consistent property grid layouts
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

	// ============================================================================
	// IMAGE & TEXTURE COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Display an image/texture
	 */
	void Image(Ref<Texture2D> texture,
			   const Size& size,
			   bool flipY = true,
			   const Color& tint = Color(1, 1, 1, 1));
	
	void Image(uint32_t textureID,
			   const Size& size,
			   bool flipY = true,
			   const Color& tint = Color(1, 1, 1, 1));
	
	/**
	 * @brief Image that acts as a button
	 */
	bool ImageButton(const std::string& id,
					 Ref<Texture2D> texture,
					 const Size& size,
					 bool flipY = true,
					 const char* tooltip = nullptr);
	
	/**
	 * @brief Texture slot for material editors etc.
	 * Includes thumbnail, label, remove button, and drag-drop support
	 */
	struct TextureSlotResult {
		bool textureChanged = false;
		bool removeClicked = false;
		std::string droppedPath;
	};
	
	TextureSlotResult TextureSlot(const std::string& label,
								  const char* icon,
								  Ref<Texture2D> currentTexture,
								  const std::string& currentPath);
	
	/**
	 * @brief Thumbnail preview (used in content browser etc.)
	 */
	void Thumbnail(Ref<Texture2D> texture,
				   const Size& size,
				   bool selected = false,
				   bool hovered = false);
	
	/**
	 * @brief Color preview button
	 */
	bool ColorPreviewButton(const std::string& id, const Color& color, const Size& size);

	// ============================================================================
	// DISABLED STATE
	// ============================================================================
	
	void BeginDisabled(bool disabled = true);
	void EndDisabled();

} // namespace Lunex::UI
