#pragma once

#include "Core/Core.h"
#include "Input/InputManager.h"
#include "Input/KeyBinding.h"
#include <string>

namespace Lunex {

	/**
	 * InputSettingsPanel - ImGui panel for remapping input bindings
	 * 
	 * Features:
	 * - Visual key binding list
	 * - Live remapping with key capture
	 * - Search/filter actions
	 * - Save/Load profiles
	 * - Reset to defaults
	 */
	class InputSettingsPanel {
	public:
		InputSettingsPanel() = default;
		~InputSettingsPanel() = default;

		void OnImGuiRender();
		
		void Open() { m_Open = true; }
		void Close() { m_Open = false; }
		void Toggle() { m_Open = !m_Open; }
		bool IsOpen() const { return m_Open; }

	private:
		void RenderActionList();
		void RenderRemapDialog();
		void RenderConfirmDialog();

		void BeginRemap(const std::string& actionName);
		void CancelRemap();
		void ApplyRemap();

		void ResetToDefaults();
		void SaveBindings();
		void LoadBindings();

		std::string GetKeyNameForAction(const std::string& actionName);

	private:
		bool m_Open = false;
		
		// Remap state
		bool m_IsRemapping = false;
		std::string m_RemapActionName;
		KeyCode m_CapturedKey = Key::Space;
		uint8_t m_CapturedModifiers = KeyModifiers::None;

		// Confirm dialog
		bool m_ShowConfirmReset = false;

		// Search filter
		char m_SearchBuffer[256] = "";

		// UI constants
		static constexpr float ACTION_COLUMN_WIDTH = 250.0f;
		static constexpr float BINDING_COLUMN_WIDTH = 150.0f;
		static constexpr float BUTTON_WIDTH = 80.0f;
	};

} // namespace Lunex
