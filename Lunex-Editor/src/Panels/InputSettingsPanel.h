/**
 * @file InputSettingsPanel.h
 * @brief Input Settings Panel - Keyboard Shortcuts Configuration
 * 
 * Features:
 * - Visual key binding list with inline remapping
 * - Click on key to remap (shows "..." during capture)
 * - Search/filter actions
 * - Save/Load bindings
 * - Reset to defaults
 */

#pragma once

#include "Core/Core.h"
#include "Input/InputManager.h"
#include "Input/KeyBinding.h"
#include <string>

namespace Lunex {

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
		// UI Sections
		void DrawToolbar();
		void RenderActionList();
		void RenderKeyBindingCell(const std::string& actionName, Ref<Action> action);
		void RenderConfirmDialog();

		// Key capture
		void CaptureKeyPress(const std::string& actionName);
		void BeginRemap(const std::string& actionName);
		void CancelRemap();
		void ApplyRemap(const std::string& actionName, KeyCode key, uint8_t modifiers);

		// Persistence
		void ResetToDefaults();
		void SaveBindings();
		void LoadBindings();

		// Helpers
		std::string GetKeyNameForAction(const std::string& actionName);

	private:
		bool m_Open = false;
		
		// Remap state
		bool m_IsRemapping = false;
		std::string m_RemapActionName;

		// Confirm dialog
		bool m_ShowConfirmReset = false;

		// Search filter
		char m_SearchBuffer[256] = "";
	};

} // namespace Lunex
