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
	
	
	// ============================================================================
	// COLOR CLASS
	// ============================================================================
	
	/**
	 * @brief Color class with conversion methods for ImGui compatibility
	 * Compatible with both .R/.G/.B/.A and .r/.g/.b/.a access
	 */
	struct Color {
		float r, g, b, a;
		
		// Constructors
		constexpr Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
		constexpr Color(float red, float green, float blue, float alpha = 1.0f) 
			: r(red), g(green), b(blue), a(alpha) {}
		constexpr Color(const glm::vec4& v) : r(v.r), g(v.g), b(v.b), a(v.a) {}
		constexpr Color(const glm::vec3& v, float alpha = 1.0f) : r(v.r), g(v.g), b(v.b), a(alpha) {}
		
		// Conversion operators
		operator glm::vec4() const { return glm::vec4(r, g, b, a); }
		operator glm::vec3() const { return glm::vec3(r, g, b); }
		
		// Data pointer for glm::value_ptr compatibility
		float* data() { return &r; }
		const float* data() const { return &r; }
		
		// ImGui conversions
		ImVec4 ToImVec4() const { return ImVec4(r, g, b, a); }
		ImU32 ToImU32() const { return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255)); }
		
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
		Color WithAlpha(float alpha) const { return Color(r, g, b, alpha); }
		Color Lighter(float amount = 0.1f) const {
			return Color(
				std::min(r + amount, 1.0f),
				std::min(g + amount, 1.0f),
				std::min(b + amount, 1.0f),
				a
			);
		}
		Color Darker(float amount = 0.1f) const {
			return Color(
				std::max(r - amount, 0.0f),
				std::max(g - amount, 0.0f),
				std::max(b - amount, 0.0f),
				a
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
	
	// ============================================================================
	// DESIGN TOKENS (Lunexi themes)
	// ============================================================================

	/**
	 * @brief Every color/metric the framework uses. A Theme is a named set of tokens;
	 * UI::Colors::* read the active theme, so switching themes restyles every
	 * component without touching them (Astro-style design tokens).
	 */
	struct ThemeTokens {
		Color Primary = Color::FromHex(0xD9822B);
		Color PrimaryHover = Color::FromHex(0xEB9645);
		Color PrimaryActive = Color::FromHex(0xB86A1F);
		Color Success = Color::FromHex(0x4CAF50);
		Color Warning = Color::FromHex(0xFFA726);
		Color Danger = Color::FromHex(0xEF5350);
		Color Info = Color::FromHex(0x42A5F5);
		Color TextPrimary = Color(0.86f, 0.86f, 0.86f, 1.0f);
		Color TextSecondary = Color(0.56f, 0.56f, 0.56f, 1.0f);
		Color TextMuted = Color(0.42f, 0.42f, 0.42f, 1.0f);
		Color TextDisabled = Color(0.30f, 0.30f, 0.30f, 1.0f);
		Color BgDark = Color::FromHex(0x151515);
		Color BgMedium = Color::FromHex(0x1A1A1A);
		Color BgLight = Color::FromHex(0x242424);
		Color BgCard = Color::FromHex(0x202020);
		Color BgHover = Color::FromHex(0x2F2F2F);
		Color Border = Color::FromHex(0x0E0E0E);
		Color BorderLight = Color::FromHex(0x383838);
		Color BorderFocus = Color(0.851f, 0.510f, 0.169f, 0.50f);
		Color AxisX = Color(0.89f, 0.22f, 0.21f, 1.0f);
		Color AxisXHover = Color(1.00f, 0.32f, 0.31f, 1.0f);
		Color AxisY = Color(0.27f, 0.75f, 0.27f, 1.0f);
		Color AxisYHover = Color(0.37f, 0.85f, 0.37f, 1.0f);
		Color AxisZ = Color(0.22f, 0.46f, 0.93f, 1.0f);
		Color AxisZHover = Color(0.32f, 0.56f, 1.00f, 1.0f);
		Color Selected = Color(0.851f, 0.510f, 0.169f, 0.18f);
		Color SelectedBorder = Color::FromHex(0xD9822B);
		Color Shadow = Color(0.0f, 0.0f, 0.0f, 0.50f);

		// Metrics
		float WindowRounding = 0.0f;
		float FrameRounding = 3.0f;
		float TabRounding = 2.0f;
		float GrabRounding = 3.0f;
		ImVec2 FramePadding = ImVec2(6.0f, 4.0f);
		ImVec2 ItemSpacing = ImVec2(8.0f, 5.0f);
	};

	// Tokens of the active theme (defined in Lunexi/Theme.cpp)
	const ThemeTokens& CurrentTokens();

	namespace Colors {
		inline Color Primary() { return CurrentTokens().Primary; }
		inline Color PrimaryHover() { return CurrentTokens().PrimaryHover; }
		inline Color PrimaryActive() { return CurrentTokens().PrimaryActive; }
		inline Color Success() { return CurrentTokens().Success; }
		inline Color Warning() { return CurrentTokens().Warning; }
		inline Color Danger() { return CurrentTokens().Danger; }
		inline Color Info() { return CurrentTokens().Info; }
		inline Color TextPrimary() { return CurrentTokens().TextPrimary; }
		inline Color TextSecondary() { return CurrentTokens().TextSecondary; }
		inline Color TextMuted() { return CurrentTokens().TextMuted; }
		inline Color TextDisabled() { return CurrentTokens().TextDisabled; }
		inline Color BgDark() { return CurrentTokens().BgDark; }
		inline Color BgMedium() { return CurrentTokens().BgMedium; }
		inline Color BgLight() { return CurrentTokens().BgLight; }
		inline Color BgCard() { return CurrentTokens().BgCard; }
		inline Color BgHover() { return CurrentTokens().BgHover; }
		inline Color Border() { return CurrentTokens().Border; }
		inline Color BorderLight() { return CurrentTokens().BorderLight; }
		inline Color BorderFocus() { return CurrentTokens().BorderFocus; }
		inline Color AxisX() { return CurrentTokens().AxisX; }
		inline Color AxisXHover() { return CurrentTokens().AxisXHover; }
		inline Color AxisY() { return CurrentTokens().AxisY; }
		inline Color AxisYHover() { return CurrentTokens().AxisYHover; }
		inline Color AxisZ() { return CurrentTokens().AxisZ; }
		inline Color AxisZHover() { return CurrentTokens().AxisZHover; }
		inline Color Selected() { return CurrentTokens().Selected; }
		inline Color SelectedBorder() { return CurrentTokens().SelectedBorder; }
		inline Color Shadow() { return CurrentTokens().Shadow; }
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
		
		constexpr float CardRounding = 4.0f;
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
