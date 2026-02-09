/**
 * @file InputSettingsPanel.cpp
 * @brief Input Settings Panel - Migrated to Lunex UI Framework
 * 
 * Features:
 * - Visual key binding list with inline remapping
 * - Click on key to remap (shows "..." during capture)
 * - Search/filter actions
 * - Save/Load bindings
 * - Reset to defaults
 */

#include "InputSettingsPanel.h"
#include "Core/Input.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>
#include <algorithm>
#include <map>

namespace Lunex {

	// ============================================================================
	// PANEL STYLE CONSTANTS
	// ============================================================================
	
	namespace InputSettingsStyle {
		// Colors
		inline UI::Color BgPanel()         { return UI::Color(0.06f, 0.06f, 0.06f, 0.95f); }
		inline UI::Color KeyBg()           { return UI::Color(0.12f, 0.12f, 0.12f, 1.0f); }
		inline UI::Color KeyText()         { return UI::Colors::Primary(); }
		inline UI::Color CaptureBg()       { return UI::Color(0.10f, 0.18f, 0.38f, 1.0f); }
		inline UI::Color CaptureText()     { return UI::Color(1.0f, 1.0f, 1.0f, 1.0f); }
		inline UI::Color CategoryText()    { return UI::Colors::Primary(); }
		inline UI::Color TableHeader()     { return UI::Color(0.10f, 0.10f, 0.10f, 1.0f); }
	}

	// ============================================================================
	// HELPER: ImGuiKey to KeyCode conversion
	// ============================================================================
	
	namespace {
		KeyCode ImGuiKeyToKeyCode(ImGuiKey key) {
			switch (key) {
				case ImGuiKey_Space: return Key::Space;
				case ImGuiKey_Apostrophe: return Key::Apostrophe;
				case ImGuiKey_Comma: return Key::Comma;
				case ImGuiKey_Minus: return Key::Minus;
				case ImGuiKey_Period: return Key::Period;
				case ImGuiKey_Slash: return Key::Slash;
				case ImGuiKey_0: return Key::D0;
				case ImGuiKey_1: return Key::D1;
				case ImGuiKey_2: return Key::D2;
				case ImGuiKey_3: return Key::D3;
				case ImGuiKey_4: return Key::D4;
				case ImGuiKey_5: return Key::D5;
				case ImGuiKey_6: return Key::D6;
				case ImGuiKey_7: return Key::D7;
				case ImGuiKey_8: return Key::D8;
				case ImGuiKey_9: return Key::D9;
				case ImGuiKey_A: return Key::A;
				case ImGuiKey_B: return Key::B;
				case ImGuiKey_C: return Key::C;
				case ImGuiKey_D: return Key::D;
				case ImGuiKey_E: return Key::E;
				case ImGuiKey_F: return Key::F;
				case ImGuiKey_G: return Key::G;
				case ImGuiKey_H: return Key::H;
				case ImGuiKey_I: return Key::I;
				case ImGuiKey_J: return Key::J;
				case ImGuiKey_K: return Key::K;
				case ImGuiKey_L: return Key::L;
				case ImGuiKey_M: return Key::M;
				case ImGuiKey_N: return Key::N;
				case ImGuiKey_O: return Key::O;
				case ImGuiKey_P: return Key::P;
				case ImGuiKey_Q: return Key::Q;
				case ImGuiKey_R: return Key::R;
				case ImGuiKey_S: return Key::S;
				case ImGuiKey_T: return Key::T;
				case ImGuiKey_U: return Key::U;
				case ImGuiKey_V: return Key::V;
				case ImGuiKey_W: return Key::W;
				case ImGuiKey_X: return Key::X;
				case ImGuiKey_Y: return Key::Y;
				case ImGuiKey_Z: return Key::Z;
				case ImGuiKey_F1: return Key::F1;
				case ImGuiKey_F2: return Key::F2;
				case ImGuiKey_F3: return Key::F3;
				case ImGuiKey_F4: return Key::F4;
				case ImGuiKey_F5: return Key::F5;
				case ImGuiKey_F6: return Key::F6;
				case ImGuiKey_F7: return Key::F7;
				case ImGuiKey_F8: return Key::F8;
				case ImGuiKey_F9: return Key::F9;
				case ImGuiKey_F10: return Key::F10;
				case ImGuiKey_F11: return Key::F11;
				case ImGuiKey_F12: return Key::F12;
				case ImGuiKey_Escape: return Key::Escape;
				case ImGuiKey_Enter: return Key::Enter;
				case ImGuiKey_Tab: return Key::Tab;
				case ImGuiKey_Backspace: return Key::Backspace;
				case ImGuiKey_Insert: return Key::Insert;
				case ImGuiKey_Delete: return Key::Delete;
				case ImGuiKey_RightArrow: return Key::Right;
				case ImGuiKey_LeftArrow: return Key::Left;
				case ImGuiKey_DownArrow: return Key::Down;
				case ImGuiKey_UpArrow: return Key::Up;
				case ImGuiKey_PageUp: return Key::PageUp;
				case ImGuiKey_PageDown: return Key::PageDown;
				case ImGuiKey_Home: return Key::Home;
				case ImGuiKey_End: return Key::End;
				case ImGuiKey_LeftShift: return Key::LeftShift;
				case ImGuiKey_LeftCtrl: return Key::LeftControl;
				case ImGuiKey_LeftAlt: return Key::LeftAlt;
				case ImGuiKey_RightShift: return Key::RightShift;
				case ImGuiKey_RightCtrl: return Key::RightControl;
				case ImGuiKey_RightAlt: return Key::RightAlt;
				case ImGuiKey_GraveAccent: return Key::GraveAccent;
				default: return Key::Space;
			}
		}
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void InputSettingsPanel::OnImGuiRender() {
		if (!m_Open) return;

		using namespace UI;

		// Window setup
		ImGui::SetNextWindowSize(ImVec2(950, 700), ImGuiCond_FirstUseEver);
		CenterNextWindow();
		
		// Window styling - push individual style vars
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);
		
		if (BeginPanel("Input Settings", &m_Open, ImGuiWindowFlags_NoCollapse)) {
			// Header
			Heading("Keyboard Shortcuts Configuration", 1);
			AddSpacing(SpacingValues::SM);
			TextStyled("Click on any key binding to remap it. Changes are saved when you click 'Save'.", TextVariant::Muted);
			
			AddSpacing(SpacingValues::MD);
			Separator();
			AddSpacing(SpacingValues::MD);

			// Toolbar
			DrawToolbar();

			AddSpacing(SpacingValues::MD);
			Separator();
			AddSpacing(SpacingValues::MD);

			// Action list
			RenderActionList();

			// Confirm reset dialog
			RenderConfirmDialog();
		}
		EndPanel();
		
		ImGui::PopStyleVar(4);
	}

	// ============================================================================
	// TOOLBAR
	// ============================================================================

	void InputSettingsPanel::DrawToolbar() {
		using namespace UI;

		// Save button
		if (Button("Save", ButtonVariant::Success, ButtonSize::Medium, Size(110, 0))) {
			SaveBindings();
		}
		if (IsItemHovered()) {
			SetTooltip("Save all changes to disk");
		}

		SameLine();

		// Load button
		if (Button("Load", ButtonVariant::Default, ButtonSize::Medium, Size(110, 0))) {
			LoadBindings();
		}
		if (IsItemHovered()) {
			SetTooltip("Reload from disk (discards unsaved changes)");
		}

		SameLine();

		// Reset button
		if (Button("Reset to Defaults", ButtonVariant::Warning, ButtonSize::Medium, Size(160, 0))) {
			m_ShowConfirmReset = true;
		}
		if (IsItemHovered()) {
			SetTooltip("Reset all shortcuts to default values");
		}

		SameLine();
		AddSpacing(SpacingValues::LG);
		SameLine();

		// Search box
		ImGui::SetNextItemWidth(280.0f);
		ScopedStyle searchStyle(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
		InputText("##Search", m_SearchBuffer, sizeof(m_SearchBuffer), "Search actions...");
	}

	// ============================================================================
	// ACTION LIST
	// ============================================================================

	void InputSettingsPanel::RenderActionList() {
		using namespace UI;

		auto& registry = InputManager::Get().GetRegistry();
		auto& keyMap = InputManager::Get().GetKeyMap();

		// Child window for list
		ScopedColor childBg(ImGuiCol_ChildBg, InputSettingsStyle::BgPanel());
		
		if (BeginChild("ActionList", Size(0, -50), true, ImGuiWindowFlags_HorizontalScrollbar)) {
			
			// Table
			if (ImGui::BeginTable("ActionTable", 3, 
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | 
				ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_HighlightHoveredColumn))
			{
				// Columns setup
				ImGui::TableSetupColumn("Action Name", ImGuiTableColumnFlags_WidthFixed, 350.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 220.0f);
				ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupScrollFreeze(0, 1);

				// Header
				{
					ScopedColor headerBg(ImGuiCol_TableHeaderBg, InputSettingsStyle::TableHeader());
					ImGui::TableHeadersRow();
				}

				// Filter and group actions
				std::string searchTerm(m_SearchBuffer);
				std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), ::tolower);

				std::map<std::string, std::vector<std::pair<std::string, Ref<Action>>>> groupedActions;
				
				for (const auto& [actionName, action] : registry.GetAllActions()) {
					// Apply search filter
					if (!searchTerm.empty()) {
						std::string lowerActionName = actionName;
						std::transform(lowerActionName.begin(), lowerActionName.end(), lowerActionName.begin(), ::tolower);
						std::string lowerDesc = action->GetDescription();
						std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

						if (lowerActionName.find(searchTerm) == std::string::npos &&
							lowerDesc.find(searchTerm) == std::string::npos) {
							continue;
						}
					}

					// Extract category
					std::string category = "Other";
					size_t dotPos = actionName.find('.');
					if (dotPos != std::string::npos) {
						category = actionName.substr(0, dotPos);
					}
					
					groupedActions[category].push_back({actionName, action});
				}

				// Render grouped actions
				for (const auto& [category, actions] : groupedActions) {
					// Category header row
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					
					{
						ScopedColor categoryColor(ImGuiCol_Text, InputSettingsStyle::CategoryText());
						ImGui::SetWindowFontScale(1.1f);
						ImGui::Text("%s", category.c_str());
						ImGui::SetWindowFontScale(1.0f);
					}

					// Action rows
					for (const auto& [actionName, action] : actions) {
						ScopedID actionID(actionName);
						ImGui::TableNextRow();

						// Column 0: Action name
						ImGui::TableNextColumn();
						Indent(15.0f);
						TextStyled(actionName, TextVariant::Primary);
						Unindent(15.0f);

						// Column 1: Key binding (clickable)
						ImGui::TableNextColumn();
						RenderKeyBindingCell(actionName, action);

						// Column 2: Description
						ImGui::TableNextColumn();
						TextStyled(action->GetDescription(), TextVariant::Muted);
					}
				}

				ImGui::EndTable();
			}
		}
		EndChild();

		// Footer
		AddSpacing(SpacingValues::SM);
		Separator();
		AddSpacing(SpacingValues::SM);
		
		TextStyled(
			"Total: " + std::to_string(registry.GetActionCount()) + " actions  |  " +
			std::to_string(keyMap.GetBindingCount()) + " bindings  |  Tip: Press Ctrl+K to open this panel",
			TextVariant::Muted
		);
	}

	// ============================================================================
	// KEY BINDING CELL
	// ============================================================================

	void InputSettingsPanel::RenderKeyBindingCell(const std::string& actionName, Ref<Action> action) {
		using namespace UI;

		bool isCapturing = (m_IsRemapping && m_RemapActionName == actionName);
		std::string bindingText = GetKeyNameForAction(actionName);
		
		// Determine display state
		std::string displayText;
		Color buttonColor;
		Color textColor;
		
		if (isCapturing) {
			displayText = "...";
			buttonColor = InputSettingsStyle::CaptureBg();
			textColor = InputSettingsStyle::CaptureText();
		}
		else if (bindingText.empty()) {
			displayText = "(Unbound)";
			buttonColor = InputSettingsStyle::KeyBg();
			textColor = Colors::TextMuted();
		}
		else {
			displayText = bindingText;
			buttonColor = InputSettingsStyle::KeyBg();
			textColor = InputSettingsStyle::KeyText();
		}

		// Render button or static text
		if (action->IsRemappable()) {
			ScopedColor btnColors({
				{ImGuiCol_Button, buttonColor},
				{ImGuiCol_ButtonHovered, Colors::Primary()},
				{ImGuiCol_Text, textColor}
			});
			
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
			
			if (ImGui::Button(displayText.c_str(), ImVec2(-1, 0))) {
				BeginRemap(actionName);
			}
			
			ImGui::PopStyleVar(2);
			
			if (IsItemHovered()) {
				SetTooltip("Click to remap this binding");
			}

			// Key capture logic
			if (isCapturing) {
				CaptureKeyPress(actionName);
			}
		}
		else {
			// Non-remappable
			TextStyled(bindingText + " (Fixed)", TextVariant::Muted);
		}
	}

	// ============================================================================
	// KEY CAPTURE
	// ============================================================================

	void InputSettingsPanel::CaptureKeyPress(const std::string& actionName) {
		ImGuiIO& io = ImGui::GetIO();
		
		for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
			ImGuiKey imguiKey = (ImGuiKey)key;
			
			// Skip modifier keys
			if (imguiKey == ImGuiKey_LeftCtrl || imguiKey == ImGuiKey_RightCtrl ||
				imguiKey == ImGuiKey_LeftShift || imguiKey == ImGuiKey_RightShift ||
				imguiKey == ImGuiKey_LeftAlt || imguiKey == ImGuiKey_RightAlt ||
				imguiKey == ImGuiKey_LeftSuper || imguiKey == ImGuiKey_RightSuper) {
				continue;
			}
			
			if (ImGui::IsKeyPressed(imguiKey, false)) {
				KeyCode capturedKey = ImGuiKeyToKeyCode(imguiKey);

				// Capture modifiers
				uint8_t capturedModifiers = KeyModifiers::None;
				if (io.KeyCtrl)  capturedModifiers |= KeyModifiers::Ctrl;
				if (io.KeyShift) capturedModifiers |= KeyModifiers::Shift;
				if (io.KeyAlt)   capturedModifiers |= KeyModifiers::Alt;
				if (io.KeySuper) capturedModifiers |= KeyModifiers::Super;

				ApplyRemap(actionName, capturedKey, capturedModifiers);
				break;
			}
		}
	}

	// ============================================================================
	// CONFIRM DIALOG
	// ============================================================================

	void InputSettingsPanel::RenderConfirmDialog() {
		if (!m_ShowConfirmReset) return;

		using namespace UI;

		OpenPopup("Confirm Reset");

		ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_Always);
		CenterNextWindow();
		
		ScopedStyle modalPadding(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
		
		if (BeginModal("Confirm Reset", nullptr, Size(500, 200), ImGuiWindowFlags_NoResize)) {
			{
				ScopedColor warningColor(ImGuiCol_Text, Colors::Warning());
				Heading("Reset to Default Bindings", 2);
			}
			
			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::MD);

			UI::TextWrapped("Are you sure you want to reset all keyboard shortcuts to their default values?");
			AddSpacing(SpacingValues::SM);
			UI::TextWrapped("This action cannot be undone. All your custom bindings will be lost.", TextVariant::Muted);
			
			AddSpacing(SpacingValues::LG);
			Separator();
			AddSpacing(SpacingValues::MD);

			// Centered buttons
			float buttonWidth = 150.0f;
			float spacing = 10.0f;
			float totalWidth = buttonWidth * 2 + spacing;
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);

			if (Button("Yes, Reset", ButtonVariant::Danger, ButtonSize::Medium, Size(buttonWidth, 0))) {
				ResetToDefaults();
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}

			SameLine(0, spacing);

			if (Button("Cancel", ButtonVariant::Default, ButtonSize::Medium, Size(buttonWidth, 0)) || 
				ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}

			EndModal();
		}
	}
	
	// ============================================================================
	// REMAP OPERATIONS
	// ============================================================================

	void InputSettingsPanel::BeginRemap(const std::string& actionName) {
		m_IsRemapping = true;
		m_RemapActionName = actionName;
	}

	void InputSettingsPanel::CancelRemap() {
		m_IsRemapping = false;
		m_RemapActionName.clear();
	}

	void InputSettingsPanel::ApplyRemap(const std::string& actionName, KeyCode key, uint8_t modifiers) {
		auto& keyMap = InputManager::Get().GetKeyMap();

		// Remove existing binding for this action
		keyMap.UnbindAction(actionName);

		// Add new binding
		keyMap.Bind(key, modifiers, actionName);

		LNX_LOG_INFO("Remapped '{0}' to {1}", actionName, 
			KeyBinding(key, modifiers, "").ToString());

		// Stop capturing
		m_IsRemapping = false;
		m_RemapActionName.clear();
	}

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	void InputSettingsPanel::ResetToDefaults() {
		InputManager::Get().ResetToDefaults();
		LNX_LOG_INFO("Reset input bindings to defaults");
	}

	void InputSettingsPanel::SaveBindings() {
		if (InputManager::Get().SaveBindings()) {
			LNX_LOG_INFO("Saved input bindings globally");
		}
		else {
			LNX_LOG_ERROR("Failed to save input bindings");
		}
	}

	void InputSettingsPanel::LoadBindings() {
		if (InputManager::Get().LoadBindings()) {
			LNX_LOG_INFO("Loaded input bindings globally");
		}
		else {
			LNX_LOG_ERROR("Failed to load input bindings");
		}
	}

	std::string InputSettingsPanel::GetKeyNameForAction(const std::string& actionName) {
		auto& keyMap = InputManager::Get().GetKeyMap();
		auto bindings = keyMap.GetBindingsFor(actionName);

		if (bindings.empty())
			return "";

		return bindings[0].ToString();
	}

} // namespace Lunex
