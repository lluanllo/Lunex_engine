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
	 * - Visual key binding list with inline remapping
	 * - Click on key to remap (shows "..." during capture)
	 * - Search/filter actions
	 * - Save/Load bindings
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
		void RenderConfirmDialog();

		void BeginRemap(const std::string& actionName);
		void CancelRemap();
		void ApplyRemap(const std::string& actionName, KeyCode key, uint8_t modifiers);

		void ResetToDefaults();
		void SaveBindings();
		void LoadBindings();

		std::string GetKeyNameForAction(const std::string& actionName);

	private:
		bool m_Open = false;
		
		// Remap state (inline capture)
		bool m_IsRemapping = false;
		std::string m_RemapActionName;

		// Confirm dialog
		bool m_ShowConfirmReset = false;

		// Search filter
		char m_SearchBuffer[256] = "";
	};

} // namespace Lunex
