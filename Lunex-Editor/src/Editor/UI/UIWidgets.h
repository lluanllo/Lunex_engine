#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <functional>

namespace Lunex::UI {

	// ============================================================================
	// UI WIDGETS - Reusable UI components for the editor
	// These provide a consistent look and feel across all panels
	// ============================================================================
	class Widgets {
	public:
		// =========================================
		// PROPERTY GRID HELPERS
		// =========================================
		
		// Begin/End property grid with consistent styling
		static void BeginPropertyGrid();
		static void EndPropertyGrid();

		// Property label with optional tooltip
		static void PropertyLabel(const char* label, const char* tooltip = nullptr);

		// Section header with icon
		static void SectionHeader(const char* icon, const char* title);

		// Separator with spacing
		static void SectionSeparator();

		// =========================================
		// PROPERTY CONTROLS
		// =========================================

		// Slider for float values
		static bool PropertySlider(const char* label, float* value, float min, float max, 
			const char* format = "%.2f", const char* tooltip = nullptr);

		// Drag for float values
		static bool PropertyDrag(const char* label, float* value, float speed = 0.1f, 
			float min = 0.0f, float max = 0.0f, const char* format = "%.2f", 
			const char* tooltip = nullptr);

		// Drag for int values
		static bool PropertyDragInt(const char* label, int* value, float speed = 1.0f,
			int min = 0, int max = 0, const char* tooltip = nullptr);

		// Color picker (RGB)
		static bool PropertyColor(const char* label, glm::vec3& color, const char* tooltip = nullptr);

		// Color picker (RGBA)
		static bool PropertyColor4(const char* label, glm::vec4& color, const char* tooltip = nullptr);

		// Checkbox
		static bool PropertyCheckbox(const char* label, bool* value, const char* tooltip = nullptr);

		// Text input
		static bool PropertyText(const char* label, std::string& value, const char* tooltip = nullptr);

		// Dropdown/Combo
		static bool PropertyCombo(const char* label, int* currentIndex, const char* const* items, 
			int itemCount, const char* tooltip = nullptr);

		// =========================================
		// VECTOR CONTROLS
		// =========================================

		// Vec3 control with colored XYZ buttons (for Transform)
		static void DrawVec3Control(const std::string& label, glm::vec3& values, 
			float resetValue = 0.0f, float columnWidth = 120.0f);

		// Vec2 control
		static bool DrawVec2Control(const char* label, glm::vec2& values, 
			float resetValue = 0.0f, float columnWidth = 120.0f);

		// =========================================
		// BUTTONS
		// =========================================

		// Primary action button (blue accent)
		static bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));

		// Danger action button (red)
		static bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

		// Success action button (green)
		static bool SuccessButton(const char* label, const ImVec2& size = ImVec2(0, 0));

		// Secondary button (gray)
		static bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));

		// Icon button (small, square)
		static bool IconButton(const char* icon, const char* tooltip = nullptr, 
			const ImVec2& size = ImVec2(25, 25));

		// =========================================
		// ASSET DROP ZONES
		// =========================================

		// Generic asset drop zone with visual feedback
		static bool AssetDropZone(const char* label, const char* acceptedTypes, 
			const ImVec2& size = ImVec2(-1, 60.0f));

		// Texture drop zone with preview
		static bool TextureDropZone(const char* label, uint32_t textureID, 
			const ImVec2& size = ImVec2(-1, 70.0f));

		// =========================================
		// CARDS & CONTAINERS
		// =========================================

		// Begin a styled card (dark background with optional border)
		static bool BeginCard(const char* id, const ImVec2& size, bool border = false,
			const ImVec4& borderColor = ImVec4(0, 0, 0, 0));
		static void EndCard();

		// Begin a collapsible tree node styled as a component header
		static bool BeginComponentHeader(const char* label, bool* removeComponent = nullptr,
			bool canRemove = true);
		static void EndComponentHeader(bool open);

		// =========================================
		// STATUS INDICATORS
		// =========================================

		// Status badge (colored text with optional icon)
		static void StatusBadge(const char* text, const ImVec4& color);

		// Progress bar with optional label
		static void ProgressBar(float progress, const ImVec2& size = ImVec2(-1, 8.0f),
			const char* overlay = nullptr);

		// =========================================
		// UTILITY
		// =========================================

		// Centered text
		static void CenteredText(const char* text);

		// Tooltip helper (call after the item you want to add a tooltip to)
		static void HelpMarker(const char* desc);

		// Draw a shadow below a rect (for card effects)
		static void DrawShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max,
			float rounding = 0.0f, float offset = 3.0f, float alpha = 0.3f);

		// Truncate text with ellipsis
		static std::string TruncateText(const std::string& text, int maxChars);
	};

} // namespace Lunex::UI
