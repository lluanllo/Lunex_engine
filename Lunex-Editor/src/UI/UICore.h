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
	// TYPE ALIASES
	// ============================================================================
	
	using Color = glm::vec4;
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
	
	inline ImVec4 ToImVec4(const Color& c) { return ImVec4(c.r, c.g, c.b, c.a); }
	inline ImVec2 ToImVec2(const Size& s) { return ImVec2(s.x, s.y); }
	inline Color FromImVec4(const ImVec4& c) { return Color(c.x, c.y, c.z, c.w); }
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
			ImGui::PushStyleColor(idx, ToImVec4(color));
		}
		
		ScopedColor(std::initializer_list<std::pair<ImGuiCol, Color>> colors) 
			: m_Count((int)colors.size()) {
			for (const auto& [idx, color] : colors) {
				ImGui::PushStyleColor(idx, ToImVec4(color));
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
		
		template<typename... Args>
		ScopedStyle(Args&&... args) : m_Count(0) {
			PushMultiple(std::forward<Args>(args)...);
		}
		
		~ScopedStyle() { ImGui::PopStyleVar(m_Count); }
		
	private:
		template<typename T, typename... Rest>
		void PushMultiple(ImGuiStyleVar idx, T val, Rest&&... rest) {
			if constexpr (std::is_same_v<T, ImVec2>) {
				ImGui::PushStyleVar(idx, val);
			} else {
				ImGui::PushStyleVar(idx, val);
			}
			m_Count++;
			if constexpr (sizeof...(rest) > 0) {
				PushMultiple(std::forward<Rest>(rest)...);
			}
		}
		
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
