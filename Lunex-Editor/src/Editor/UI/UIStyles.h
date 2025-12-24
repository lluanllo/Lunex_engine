#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

namespace Lunex::UI {

	// ============================================================================
	// UI STYLE CONSTANTS - Consistent styling across the entire editor
	// Inspired by Unreal Engine and Unity editor aesthetics
	// ============================================================================
	namespace Style {
		// Layout
		constexpr float SECTION_SPACING = 8.0f;
		constexpr float INDENT_SIZE = 12.0f;
		constexpr float HEADER_HEIGHT = 28.0f;
		constexpr float THUMBNAIL_SIZE = 64.0f;
		constexpr float COLUMN_WIDTH = 120.0f;
		constexpr float BUTTON_HEIGHT = 32.0f;
		constexpr float CARD_ROUNDING = 6.0f;
		constexpr float CARD_PADDING = 8.0f;

		// Text Colors
		inline const ImVec4 COLOR_HEADER = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		inline const ImVec4 COLOR_SUBHEADER = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
		inline const ImVec4 COLOR_HINT = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		inline const ImVec4 COLOR_TEXT_PRIMARY = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
		inline const ImVec4 COLOR_TEXT_SECONDARY = ImVec4(0.80f, 0.80f, 0.82f, 1.0f);

		// Accent Colors
		inline const ImVec4 COLOR_ACCENT = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
		inline const ImVec4 COLOR_ACCENT_HOVER = ImVec4(0.36f, 0.69f, 1.0f, 1.0f);
		inline const ImVec4 COLOR_ACCENT_ACTIVE = ImVec4(0.20f, 0.50f, 0.90f, 1.0f);

		// Status Colors
		inline const ImVec4 COLOR_SUCCESS = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
		inline const ImVec4 COLOR_WARNING = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
		inline const ImVec4 COLOR_DANGER = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
		inline const ImVec4 COLOR_INFO = ImVec4(0.3f, 0.6f, 0.9f, 1.0f);

		// Background Colors
		inline const ImVec4 COLOR_BG_DARK = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
		inline const ImVec4 COLOR_BG_MEDIUM = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
		inline const ImVec4 COLOR_BG_LIGHT = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
		inline const ImVec4 COLOR_BG_WINDOW = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
		inline const ImVec4 COLOR_BG_PANEL = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);

		// Border Colors
		inline const ImVec4 COLOR_BORDER = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
		inline const ImVec4 COLOR_BORDER_HOVER = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
		inline const ImVec4 COLOR_BORDER_SELECTED = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);

		// Component-specific Colors (for transform gizmos, etc.)
		inline const ImVec4 COLOR_X_AXIS = ImVec4(0.70f, 0.20f, 0.20f, 1.0f);
		inline const ImVec4 COLOR_X_AXIS_HOVER = ImVec4(0.80f, 0.30f, 0.30f, 1.0f);
		inline const ImVec4 COLOR_Y_AXIS = ImVec4(0.20f, 0.70f, 0.20f, 1.0f);
		inline const ImVec4 COLOR_Y_AXIS_HOVER = ImVec4(0.30f, 0.80f, 0.30f, 1.0f);
		inline const ImVec4 COLOR_Z_AXIS = ImVec4(0.20f, 0.40f, 0.90f, 1.0f);
		inline const ImVec4 COLOR_Z_AXIS_HOVER = ImVec4(0.30f, 0.50f, 1.0f, 1.0f);

		// Asset Type Colors (for content browser borders)
		inline const ImVec4 COLOR_ASSET_MESH = ImVec4(0.39f, 0.70f, 0.39f, 1.0f);       // Green
		inline const ImVec4 COLOR_ASSET_MATERIAL = ImVec4(0.86f, 0.59f, 0.20f, 1.0f);   // Orange
		inline const ImVec4 COLOR_ASSET_ANIMATION = ImVec4(0.70f, 0.39f, 0.86f, 1.0f);  // Purple
		inline const ImVec4 COLOR_ASSET_SKELETON = ImVec4(0.39f, 0.59f, 0.86f, 1.0f);   // Blue
		inline const ImVec4 COLOR_ASSET_PREFAB = ImVec4(0.39f, 0.78f, 0.86f, 1.0f);     // Cyan
		inline const ImVec4 COLOR_ASSET_TEXTURE = ImVec4(0.86f, 0.78f, 0.39f, 1.0f);    // Yellow
		inline const ImVec4 COLOR_ASSET_SCENE = ImVec4(0.86f, 0.39f, 0.59f, 1.0f);      // Pink
	}

	// ============================================================================
	// SCOPED STYLE HELPERS
	// ============================================================================
	
	// RAII helper for ImGui style push/pop
	class ScopedStyle {
	public:
		ScopedStyle() = default;
		~ScopedStyle() {
			if (m_ColorCount > 0) ImGui::PopStyleColor(m_ColorCount);
			if (m_VarCount > 0) ImGui::PopStyleVar(m_VarCount);
		}

		void PushColor(ImGuiCol idx, const ImVec4& color) {
			ImGui::PushStyleColor(idx, color);
			m_ColorCount++;
		}

		void PushVar(ImGuiStyleVar idx, float val) {
			ImGui::PushStyleVar(idx, val);
			m_VarCount++;
		}

		void PushVar(ImGuiStyleVar idx, const ImVec2& val) {
			ImGui::PushStyleVar(idx, val);
			m_VarCount++;
		}

		// Non-copyable
		ScopedStyle(const ScopedStyle&) = delete;
		ScopedStyle& operator=(const ScopedStyle&) = delete;

	private:
		int m_ColorCount = 0;
		int m_VarCount = 0;
	};

	// RAII helper for ImGui ID scope
	class ScopedID {
	public:
		explicit ScopedID(const char* id) { ImGui::PushID(id); }
		explicit ScopedID(int id) { ImGui::PushID(id); }
		explicit ScopedID(const void* ptr) { ImGui::PushID(ptr); }
		~ScopedID() { ImGui::PopID(); }

		ScopedID(const ScopedID&) = delete;
		ScopedID& operator=(const ScopedID&) = delete;
	};

	// RAII helper for disabled state
	class ScopedDisabled {
	public:
		explicit ScopedDisabled(bool disabled = true) : m_Disabled(disabled) {
			if (m_Disabled) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			}
		}
		~ScopedDisabled() {
			if (m_Disabled) {
				ImGui::PopStyleVar();
				ImGui::PopItemFlag();
			}
		}

		ScopedDisabled(const ScopedDisabled&) = delete;
		ScopedDisabled& operator=(const ScopedDisabled&) = delete;

	private:
		bool m_Disabled;
	};

} // namespace Lunex::UI
