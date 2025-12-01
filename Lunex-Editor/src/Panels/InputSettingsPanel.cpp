#include "InputSettingsPanel.h"
#include "Core/Input.h"
#include "Log/Log.h"
#include <imgui.h>
#include <algorithm>

namespace Lunex {

	// Modern UI Style constants with better contrast and readability
	namespace {
		const ImVec4 COLOR_HEADER = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
		const ImVec4 COLOR_SUBHEADER = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
		const ImVec4 COLOR_HINT = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
		const ImVec4 COLOR_ACCENT = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);
		const ImVec4 COLOR_ACCENT_HOVER = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
		const ImVec4 COLOR_SUCCESS = ImVec4(0.2f, 0.7f, 0.3f, 1.0f);
		const ImVec4 COLOR_SUCCESS_HOVER = ImVec4(0.3f, 0.8f, 0.4f, 1.0f);
		const ImVec4 COLOR_WARNING = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
		const ImVec4 COLOR_WARNING_HOVER = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
		const ImVec4 COLOR_DANGER = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
		const ImVec4 COLOR_DANGER_HOVER = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
		const ImVec4 COLOR_KEY_BG = ImVec4(0.15f, 0.2f, 0.25f, 1.0f);
		const ImVec4 COLOR_KEY_TEXT = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
		const ImVec4 COLOR_CAPTURE_BG = ImVec4(0.25f, 0.35f, 0.45f, 1.0f);
		const ImVec4 COLOR_CAPTURE_TEXT = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		
		// Convert ImGuiKey to Lunex KeyCode
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
				default: return Key::Space; // Fallback
			}
		}
	}

	void InputSettingsPanel::OnImGuiRender() {
		if (!m_Open)
			return;

		// Better default size and positioning
		ImGui::SetNextWindowSize(ImVec2(950, 700), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		
		// Modern window styling
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);
		
		if (ImGui::Begin("Input Settings", &m_Open, ImGuiWindowFlags_NoCollapse)) {
			// Modern header with better typography
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HEADER);
			ImGui::SetWindowFontScale(1.2f);
			ImGui::Text("Keyboard Shortcuts Configuration");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
			ImGui::TextWrapped("Click on any key binding to remap it. Changes are saved when you click 'Save'.");
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Modern toolbar with better button styling
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_SUCCESS);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR_SUCCESS_HOVER);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
			if (ImGui::Button("Save", ImVec2(110, 0))) {
				SaveBindings();
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Save all changes to disk");
			}

			ImGui::SameLine();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
			if (ImGui::Button("Load", ImVec2(110, 0))) {
				LoadBindings();
			}
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Reload from disk (discards unsaved changes)");
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_WARNING);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR_WARNING_HOVER);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
			if (ImGui::Button("Reset to Defaults", ImVec2(160, 0))) {
				m_ShowConfirmReset = true;
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Reset all shortcuts to default values");
			}

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();

			// Modern search box
			ImGui::SetNextItemWidth(280.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
			ImGui::InputTextWithHint("##Search", "Search actions...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));
			ImGui::PopStyleVar();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Action list
			RenderActionList();

			// Confirm reset dialog
			RenderConfirmDialog();
		}
		ImGui::End();
		
		ImGui::PopStyleVar(4);
	}

	void InputSettingsPanel::RenderActionList() {
		auto& registry = InputManager::Get().GetRegistry();
		auto& keyMap = InputManager::Get().GetKeyMap();

		// Better child window styling
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.1f, 0.95f));
		ImGui::BeginChild("ActionList", ImVec2(0, -50), true, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::PopStyleColor();

		// Modern table with better flags
		if (ImGui::BeginTable("ActionTable", 3, 
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | 
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_HighlightHoveredColumn))
		{
			// Setup columns
			ImGui::TableSetupColumn("Action Name", ImGuiTableColumnFlags_WidthFixed, 350.0f);
			ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 220.0f);
			ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row

			// Better header styling
			ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
			ImGui::TableHeadersRow();
			ImGui::PopStyleColor();

			// Filter actions
			std::string searchTerm(m_SearchBuffer);
			std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), ::tolower);

			// Group actions by category for better organization
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

				// Extract category from action name (e.g., "Camera.MoveForward" -> "Camera")
				std::string category = "Other";
				size_t dotPos = actionName.find('.');
				if (dotPos != std::string::npos) {
					category = actionName.substr(0, dotPos);
				}
				
				groupedActions[category].push_back({actionName, action});
			}

			// Render grouped actions
			for (const auto& [category, actions] : groupedActions) {
				// Category header
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACCENT);
				ImGui::SetWindowFontScale(1.1f);
				ImGui::Text("%s", category.c_str());
				ImGui::SetWindowFontScale(1.0f);
				ImGui::PopStyleColor();

				// Render actions in this category
				for (const auto& [actionName, action] : actions) {
					ImGui::PushID(actionName.c_str());
					ImGui::TableNextRow();

					// Column 0: Action name
					ImGui::TableNextColumn();
					ImGui::Indent(15.0f);
					ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HEADER);
					ImGui::Text("%s", actionName.c_str());
					ImGui::PopStyleColor();
					ImGui::Unindent(15.0f);

					// Column 1: Current binding (CLICKABLE for remapping)
					ImGui::TableNextColumn();
					
					bool isCapturing = (m_IsRemapping && m_RemapActionName == actionName);
					std::string bindingText = GetKeyNameForAction(actionName);
					
					// Display text based on state
					std::string displayText;
					ImVec4 buttonColor;
					ImVec4 textColor;
					
					if (isCapturing) {
						// Show "..." while capturing
						displayText = "...";
						buttonColor = COLOR_CAPTURE_BG;
						textColor = COLOR_CAPTURE_TEXT;
					}
					else if (bindingText.empty()) {
						displayText = "(Unbound)";
						buttonColor = COLOR_KEY_BG;
						textColor = COLOR_HINT;
					}
					else {
						displayText = bindingText;
						buttonColor = COLOR_KEY_BG;
						textColor = COLOR_KEY_TEXT;
					}

					// Render clickable button
					if (action->IsRemappable()) {
						ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR_ACCENT);
						ImGui::PushStyleColor(ImGuiCol_Text, textColor);
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
						
						if (ImGui::Button(displayText.c_str(), ImVec2(-1, 0))) {
							// Start capturing for this action
							BeginRemap(actionName);
						}
						
						ImGui::PopStyleVar(2);
						ImGui::PopStyleColor(3);
						
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Click to remap this binding");
						}

						// Key capture logic (only when capturing this action)
						if (isCapturing) {
							ImGuiIO& io = ImGui::GetIO();
							
							// Iterate through valid ImGui named keys
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
									// Convert ImGuiKey to KeyCode
									KeyCode capturedKey = ImGuiKeyToKeyCode(imguiKey);

									// Capture modifiers
									uint8_t capturedModifiers = KeyModifiers::None;
									if (io.KeyCtrl)  capturedModifiers |= KeyModifiers::Ctrl;
									if (io.KeyShift) capturedModifiers |= KeyModifiers::Shift;
									if (io.KeyAlt)   capturedModifiers |= KeyModifiers::Alt;
									if (io.KeySuper) capturedModifiers |= KeyModifiers::Super;

									// Apply the new binding
									ApplyRemap(actionName, capturedKey, capturedModifiers);
									break;
								}
							}
						}
					}
					else {
						// Non-remappable - show as static text
						ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
						ImGui::Text("%s (Fixed)", bindingText.c_str());
						ImGui::PopStyleColor();
					}

					// Column 2: Description
					ImGui::TableNextColumn();
					ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
					ImGui::TextWrapped("%s", action->GetDescription().c_str());
					ImGui::PopStyleColor();

					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();

		// Modern footer with better info display
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
		ImGui::Text("Total: %d actions  |  %d bindings  |  Tip: Press Ctrl+K to open this panel", 
			(int)registry.GetActionCount(), (int)keyMap.GetBindingCount());
		ImGui::PopStyleColor();
	}

	void InputSettingsPanel::RenderConfirmDialog() {
		if (!m_ShowConfirmReset)
			return;

		ImGui::OpenPopup("Confirm Reset");

		// Better dialog styling
		ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
		
		if (ImGui::BeginPopupModal("Confirm Reset", nullptr, ImGuiWindowFlags_NoResize)) {
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_WARNING);
			ImGui::SetWindowFontScale(1.2f);
			ImGui::Text("Reset to Default Bindings");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextWrapped("Are you sure you want to reset all keyboard shortcuts to their default values?");
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
			ImGui::TextWrapped("This action cannot be undone. All your custom bindings will be lost.");
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Better button layout
			float buttonWidth = 150.0f;
			float spacing = 10.0f;
			float totalWidth = buttonWidth * 2 + spacing;
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);

			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_DANGER);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR_DANGER_HOVER);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
			if (ImGui::Button("Yes, Reset", ImVec2(buttonWidth, 0))) {
				ResetToDefaults();
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);

			ImGui::SameLine(0, spacing);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
			if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleVar();

			ImGui::EndPopup();
		}
		
		ImGui::PopStyleVar();
	}
	
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
		
		// UI will automatically refresh on next frame
	}

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

		// Return first binding
		return bindings[0].ToString();
	}

} // namespace Lunex
