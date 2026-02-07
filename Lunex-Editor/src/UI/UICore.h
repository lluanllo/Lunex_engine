/**
 * @file UICore.h
 * @brief Lunex UI Framework - Core definitions and base types
 * 
 * A high-level abstraction layer over ImGui providing:
 * - Consistent styling across the editor
 * - Reusable UI components
 * - Theme management
 * - Declarative-style UI building
 * 
 * Architecture inspired by modern web frameworks (React, Vue)
 * with AAA game engine UI patterns.
 */

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

#include "Renderer/Texture.h"
#include "Core/Core.h"

namespace Lunex::UI {

	// ============================================================================
	// FORWARD DECLARATIONS
	// ============================================================================
	
	class Theme;
	class Component;
	
	// ============================================================================
	// COLOR CLASS
	// ============================================================================
	
	/**
	 * @brief Color class with conversion methods for ImGui compatibility
	 * Compatible with both .R/.G/.B/.A and .r/.g/.b/.a access
	 */
	struct Color {
		union {
			struct { float R, G, B, A; };
			struct { float r, g, b, a; };  // Lowercase aliases for compatibility
		};
		
		// Constructors
		constexpr Color() : R(0.0f), G(0.0f), B(0.0f), A(1.0f) {}
		constexpr Color(float red, float green, float blue, float alpha = 1.0f) 
			: R(red), G(green), B(blue), A(alpha) {}
		constexpr Color(const glm::vec4& v) : R(v.r), G(v.g), B(v.b), A(v.a) {}
		constexpr Color(const glm::vec3& v, float alpha = 1.0f) : R(v.r), G(v.g), B(v.b), A(alpha) {}
		
		// Conversion operators
		operator glm::vec4() const { return glm::vec4(R, G, B, A); }
		operator glm::vec3() const { return glm::vec3(R, G, B); }
		
		// Data pointer for glm::value_ptr compatibility
		float* data() { return &R; }
		const float* data() const { return &R; }
		
		// ImGui conversions
		ImVec4 ToImVec4() const { return ImVec4(R, G, B, A); }
		ImU32 ToImU32() const { return IM_COL32((int)(R * 255), (int)(G * 255), (int)(B * 255), (int)(A * 255)); }
		
		// Static constructors
		static constexpr Color FromHex(uint32_t hex, float alpha = 1.0f) {
			return Color(
				((hex >> 16) & 0xFF) / 255.0f,
				((hex >> 8) & 0xFF) / 255.0f,
				(hex & 0xFF) / 255.0f,
				alpha
			);
		}
		
		static Color FromImVec4(const ImVec4& c) { return Color(c.x, c.y, c.z, c.w); }
		static Color FromImU32(ImU32 c) {
			return Color(
				((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
				((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
				((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
				((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
			);
		}
		
		// Color math
		Color WithAlpha(float alpha) const { return Color(R, G, B, alpha); }
		Color Lighter(float amount = 0.1f) const {
			return Color(
				std::min(R + amount, 1.0f),
				std::min(G + amount, 1.0f),
				std::min(B + amount, 1.0f),
				A
			);
		}
		Color Darker(float amount = 0.1f) const {
			return Color(
				std::max(R - amount, 0.0f),
				std::max(G - amount, 0.0f),
				std::max(B - amount, 0.0f),
				A
			);
		}
	};
	
	// ============================================================================
	// TYPE ALIASES
	// ============================================================================
	
	using Color3 = glm::vec3;
	using Size = glm::vec2;
	using Position = glm::vec2;
	
	// Callback types
	using OnClickCallback = std::function<void()>;
	using OnChangeCallback = std::function<void()>;
	using OnDragDropCallback = std::function<void(const void* data, size_t size)>;
	
	// ============================================================================
	// THEME COLORS - Semantic color definitions
	// ============================================================================
	
	namespace Colors {
		// Primary palette
		inline Color Primary()      { return Color(0.26f, 0.59f, 0.98f, 1.0f); }
		inline Color PrimaryHover() { return Color(0.36f, 0.69f, 1.0f, 1.0f); }
		inline Color PrimaryActive(){ return Color(0.20f, 0.50f, 0.90f, 1.0f); }
		
		// Semantic colors
		inline Color Success()      { return Color(0.30f, 0.80f, 0.30f, 1.0f); }
		inline Color Warning()      { return Color(0.80f, 0.60f, 0.20f, 1.0f); }
		inline Color Danger()       { return Color(0.80f, 0.30f, 0.30f, 1.0f); }
		inline Color Info()         { return Color(0.26f, 0.59f, 0.98f, 1.0f); }
		
		// Text colors
		inline Color TextPrimary()  { return Color(0.95f, 0.95f, 0.95f, 1.0f); }
		inline Color TextSecondary(){ return Color(0.70f, 0.70f, 0.70f, 1.0f); }
		inline Color TextMuted()    { return Color(0.50f, 0.50f, 0.50f, 1.0f); }
		inline Color TextDisabled() { return Color(0.40f, 0.40f, 0.40f, 1.0f); }
		
		// Background colors
		inline Color BgDark()       { return Color(0.10f, 0.10f, 0.11f, 1.0f); }
		inline Color BgMedium()     { return Color(0.14f, 0.14f, 0.15f, 1.0f); }
		inline Color BgLight()      { return Color(0.18f, 0.18f, 0.19f, 1.0f); }
		inline Color BgCard()       { return Color(0.16f, 0.16f, 0.17f, 1.0f); }
		inline Color BgHover()      { return Color(0.22f, 0.22f, 0.24f, 1.0f); }
		
		// Border colors
		inline Color Border()       { return Color(0.08f, 0.08f, 0.09f, 1.0f); }
		inline Color BorderLight()  { return Color(0.20f, 0.20f, 0.22f, 1.0f); }
		inline Color BorderFocus()  { return Color(0.26f, 0.59f, 0.98f, 0.50f); }
		
		// Axis colors (for Vec3 controls)
		inline Color AxisX()        { return Color(0.70f, 0.20f, 0.20f, 1.0f); }
		inline Color AxisXHover()   { return Color(0.80f, 0.30f, 0.30f, 1.0f); }
		inline Color AxisY()        { return Color(0.20f, 0.70f, 0.20f, 1.0f); }
		inline Color AxisYHover()   { return Color(0.30f, 0.80f, 0.30f, 1.0f); }
		inline Color AxisZ()        { return Color(0.20f, 0.40f, 0.90f, 1.0f); }
		inline Color AxisZHover()   { return Color(0.30f, 0.50f, 1.0f, 1.0f); }
		
		// Selection colors
		inline Color Selected()     { return Color(0.26f, 0.59f, 0.98f, 0.35f); }
		inline Color SelectedBorder(){ return Color(0.26f, 0.59f, 0.98f, 1.0f); }
		
		// Shadow
		inline Color Shadow()       { return Color(0.0f, 0.0f, 0.0f, 0.50f); }
	}
	
	// ============================================================================
	// SPACING & SIZING CONSTANTS
	// ============================================================================
	
	namespace SpacingValues {
		constexpr float None = 0.0f;
		constexpr float XS = 2.0f;
		constexpr float SM = 4.0f;
		constexpr float MD = 8.0f;
		constexpr float LG = 12.0f;
		constexpr float XL = 16.0f;
		constexpr float XXL = 24.0f;
		
		// Common sizes
		constexpr float IconSM = 16.0f;
		constexpr float IconMD = 20.0f;
		constexpr float IconLG = 24.0f;
		constexpr float IconXL = 32.0f;
		
		constexpr float ButtonHeight = 28.0f;
		constexpr float ButtonHeightLG = 35.0f;
		constexpr float InputHeight = 24.0f;
		
		constexpr float ThumbnailSM = 48.0f;
		constexpr float ThumbnailMD = 64.0f;
		constexpr float ThumbnailLG = 96.0f;
		constexpr float ThumbnailXL = 128.0f;
		
		constexpr float PropertyLabelWidth = 120.0f;
		constexpr float SectionIndent = 12.0f;
		
		constexpr float CardRounding = 6.0f;
		constexpr float ButtonRounding = 4.0f;
		constexpr float InputRounding = 3.0f;
	}

	// ============================================================================
	// STYLE VARIANTS
	// ============================================================================
	
	enum class ButtonVariant {
		Default,
		Primary,
		Success,
		Warning,
		Danger,
		Ghost,      // Transparent background
		Outline     // Border only
	};
	
	enum class ButtonSize {
		Small,
		Medium,
		Large
	};
	
	enum class TextVariant {
		Default,
		Primary,    // White/bright
		Secondary,  // Gray
		Muted,      // Darker gray
		Success,
		Warning,
		Danger
	};
	
	enum class InputVariant {
		Default,
		Filled,     // Darker background
		Outline     // Border only
	};

	// ============================================================================
	// DRAG & DROP PAYLOAD TYPES
	// ============================================================================
	
	constexpr const char* PAYLOAD_CONTENT_BROWSER_ITEM = "CONTENT_BROWSER_ITEM";
	constexpr const char* PAYLOAD_CONTENT_BROWSER_ITEMS = "CONTENT_BROWSER_ITEMS";
	constexpr const char* PAYLOAD_ENTITY_NODE = "ENTITY_NODE";
	constexpr const char* PAYLOAD_TEXTURE = "TEXTURE_ASSET";
	constexpr const char* PAYLOAD_MATERIAL = "MATERIAL_ASSET";
	constexpr const char* PAYLOAD_MESH = "MESH_ASSET";
	
	// ============================================================================
	// UTILITY FUNCTIONS
	// ============================================================================
	
	inline ImVec4 ToImVec4(const Color& c) { return c.ToImVec4(); }
	inline ImVec2 ToImVec2(const Size& s) { return ImVec2(s.x, s.y); }
	inline Color FromImVec4(const ImVec4& c) { return Color::FromImVec4(c); }
	inline Size FromImVec2(const ImVec2& s) { return Size(s.x, s.y); }
	
	// ============================================================================
	// SCOPED STYLE HELPERS
	// ============================================================================
	
	/**
	 * @brief RAII wrapper for ImGui style colors
	 */
	class ScopedColor {
	public:
		ScopedColor(ImGuiCol idx, const Color& color) : m_Count(1) {
			ImGui::PushStyleColor(idx, color.ToImVec4());
		}
		
		ScopedColor(ImGuiCol idx, const ImVec4& color) : m_Count(1) {
			ImGui::PushStyleColor(idx, color);
		}
		
		ScopedColor(std::initializer_list<std::pair<ImGuiCol, Color>> colors) 
			: m_Count((int)colors.size()) {
			for (const auto& [idx, color] : colors) {
				ImGui::PushStyleColor(idx, color.ToImVec4());
			}
		}
		
		ScopedColor(std::initializer_list<std::pair<ImGuiCol, ImVec4>> colors)
			: m_Count((int)colors.size()) {
			for (const auto& [idx, color] : colors) {
				ImGui::PushStyleColor(idx, color);
			}
		}
		
		~ScopedColor() { ImGui::PopStyleColor(m_Count); }
		
	private:
		int m_Count;
	};
	
	/**
	 * @brief RAII wrapper for ImGui style vars
	 */
	class ScopedStyle {
	public:
		ScopedStyle(ImGuiStyleVar idx, float val) : m_Count(1) {
			ImGui::PushStyleVar(idx, val);
		}
		
		ScopedStyle(ImGuiStyleVar idx, const ImVec2& val) : m_Count(1) {
			ImGui::PushStyleVar(idx, val);
		}
		
		// Variadic constructor for multiple style vars of same type
		ScopedStyle(ImGuiStyleVar idx1, float val1, ImGuiStyleVar idx2, float val2) : m_Count(2) {
			ImGui::PushStyleVar(idx1, val1);
			ImGui::PushStyleVar(idx2, val2);
		}
		
		ScopedStyle(ImGuiStyleVar idx1, float val1, ImGuiStyleVar idx2, float val2, 
					ImGuiStyleVar idx3, float val3) : m_Count(3) {
			ImGui::PushStyleVar(idx1, val1);
			ImGui::PushStyleVar(idx2, val2);
			ImGui::PushStyleVar(idx3, val3);
		}
		
		ScopedStyle(ImGuiStyleVar idx1, float val1, ImGuiStyleVar idx2, float val2,
					ImGuiStyleVar idx3, float val3, ImGuiStyleVar idx4, float val4) : m_Count(4) {
			ImGui::PushStyleVar(idx1, val1);
			ImGui::PushStyleVar(idx2, val2);
			ImGui::PushStyleVar(idx3, val3);
			ImGui::PushStyleVar(idx4, val4);
		}
		
		ScopedStyle(ImGuiStyleVar idx1, const ImVec2& val1, ImGuiStyleVar idx2, const ImVec2& val2) : m_Count(2) {
			ImGui::PushStyleVar(idx1, val1);
			ImGui::PushStyleVar(idx2, val2);
		}
		
		ScopedStyle(std::initializer_list<std::pair<ImGuiStyleVar, float>> vars)
			: m_Count((int)vars.size()) {
			for (const auto& [idx, val] : vars) {
				ImGui::PushStyleVar(idx, val);
			}
		}
		
		ScopedStyle(std::initializer_list<std::pair<ImGuiStyleVar, ImVec2>> vars)
			: m_Count((int)vars.size()) {
			for (const auto& [idx, val] : vars) {
				ImGui::PushStyleVar(idx, val);
			}
		}
		
		~ScopedStyle() { ImGui::PopStyleVar(m_Count); }
		
	private:
		int m_Count;
	};
	
	/**
	 * @brief RAII wrapper for ImGui ID scope
	 */
	class ScopedID {
	public:
		ScopedID(const char* str_id) { ImGui::PushID(str_id); }
		ScopedID(const std::string& str_id) { ImGui::PushID(str_id.c_str()); }
		ScopedID(int int_id) { ImGui::PushID(int_id); }
		ScopedID(const void* ptr_id) { ImGui::PushID(ptr_id); }
		~ScopedID() { ImGui::PopID(); }
	};
	
	/**
	 * @brief RAII wrapper for disabled state
	 */
	class ScopedDisabled {
	public:
		ScopedDisabled(bool disabled = true) : m_Disabled(disabled) {
			if (m_Disabled) ImGui::BeginDisabled();
		}
		~ScopedDisabled() {
			if (m_Disabled) ImGui::EndDisabled();
		}
	private:
		bool m_Disabled;
	};
	
	/**
	 * @brief RAII wrapper for font
	 */
	class ScopedFont {
	public:
		ScopedFont(ImFont* font) { 
			if (font) ImGui::PushFont(font); 
			m_Pushed = (font != nullptr);
		}
		~ScopedFont() { 
			if (m_Pushed) ImGui::PopFont(); 
		}
	private:
		bool m_Pushed = false;
	};

} // namespace Lunex::UI
