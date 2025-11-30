#include "InputSettingsPanel.h"
#include "Core/Input.h"
#include "Log/Log.h"
#include <imgui.h>
#include <algorithm>

namespace Lunex {

	// UI Style constants
	namespace {
		const ImVec4 COLOR_HEADER = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		const ImVec4 COLOR_SUBHEADER = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
		const ImVec4 COLOR_HINT = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		const ImVec4 COLOR_ACCENT = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
		const ImVec4 COLOR_SUCCESS = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
		const ImVec4 COLOR_WARNING = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
		const ImVec4 COLOR_DANGER = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
	}

	void InputSettingsPanel::OnImGuiRender() {
		if (!m_Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
		
		if (ImGui::Begin("?? Input Settings", &m_Open)) {
			// Header
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HEADER);
			ImGui::Text("Keyboard Shortcuts");
			ImGui::PopStyleColor();
			ImGui::Separator();
			ImGui::Spacing();

			// Toolbar
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_SUCCESS);
			if (ImGui::Button("?? Save", ImVec2(BUTTON_WIDTH, 0))) {
				SaveBindings();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			if (ImGui::Button("?? Load", ImVec2(BUTTON_WIDTH, 0))) {
				LoadBindings();
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_WARNING);
			if (ImGui::Button("?? Reset", ImVec2(BUTTON_WIDTH, 0))) {
				m_ShowConfirmReset = true;
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();

			// Search box
			ImGui::SetNextItemWidth(200.0f);
			ImGui::InputTextWithHint("##Search", "?? Search actions...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Action list
			RenderActionList();

			// Remap dialog
			RenderRemapDialog();

			// Confirm reset dialog
			RenderConfirmDialog();
		}
		ImGui::End();
	}

	void InputSettingsPanel::RenderActionList() {
		auto& registry = InputManager::Get().GetRegistry();
		auto& keyMap = InputManager::Get().GetKeyMap();

		// Table header
		ImGui::BeginChild("ActionList", ImVec2(0, -35), true);

		ImGui::Columns(3, "ActionColumns", true);
		ImGui::SetColumnWidth(0, ACTION_COLUMN_WIDTH);
		ImGui::SetColumnWidth(1, BINDING_COLUMN_WIDTH);

		ImGui::PushStyleColor(ImGuiCol_Text, COLOR_SUBHEADER);
		ImGui::Text("Action");
		ImGui::NextColumn();
		ImGui::Text("Current Binding");
		ImGui::NextColumn();
		ImGui::Text("Controls");
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::NextColumn();

		// Filter actions
		std::string searchTerm(m_SearchBuffer);
		std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), ::tolower);

		// List actions
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

			ImGui::PushID(actionName.c_str());

			// Action name + description
			ImGui::Text("%s", actionName.c_str());
			if (ImGui::IsItemHovered() && !action->GetDescription().empty()) {
				ImGui::SetTooltip("%s", action->GetDescription().c_str());
			}
			ImGui::NextColumn();

			// Current binding
			std::string bindingText = GetKeyNameForAction(actionName);
			
			if (bindingText.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
				ImGui::Text("(Unbound)");
				ImGui::PopStyleColor();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACCENT);
				ImGui::Text("%s", bindingText.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::NextColumn();

			// Remap button
			if (action->IsRemappable()) {
				if (ImGui::Button("Remap", ImVec2(BUTTON_WIDTH, 0))) {
					BeginRemap(actionName);
				}

				ImGui::SameLine();
				if (!bindingText.empty()) {
					ImGui::PushStyleColor(ImGuiCol_Button, COLOR_DANGER);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
					if (ImGui::Button("Clear", ImVec2(60, 0))) {
						keyMap.UnbindAction(actionName);
					}
					ImGui::PopStyleColor(2);
				}
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
				ImGui::Text("(Fixed)");
				ImGui::PopStyleColor();
			}

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);
		ImGui::EndChild();

		// Footer info
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
		ImGui::Text("Total: %d actions | %d bindings", 
			(int)registry.GetActionCount(), (int)keyMap.GetBindingCount());
		ImGui::PopStyleColor();
	}

	void InputSettingsPanel::RenderRemapDialog() {
		if (!m_IsRemapping)
			return;

		ImGui::OpenPopup("Remap Key");

		ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Remap Key", nullptr, ImGuiWindowFlags_NoResize)) {
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HEADER);
			ImGui::Text("Remapping: %s", m_RemapActionName.c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACCENT);
			ImGui::TextWrapped("Press any key combination...");
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Spacing();

			if (m_CapturedKey != Key::Space) { // Changed from Key::Unknown
				KeyBinding preview(m_CapturedKey, m_CapturedModifiers, "");
				
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_SUCCESS);
				ImGui::Text("Captured: %s", preview.ToString().c_str());
				ImGui::PopStyleColor();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_HINT);
				ImGui::Text("Waiting for input...");
				ImGui::PopStyleColor();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Buttons
			if (m_CapturedKey != Key::Space) { // Changed from Key::Unknown
				ImGui::PushStyleColor(ImGuiCol_Button, COLOR_SUCCESS);
				if (ImGui::Button("? Apply", ImVec2(120, 0))) {
					ApplyRemap();
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
			}

			if (ImGui::Button("? Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				CancelRemap();
				ImGui::CloseCurrentPopup();
			}

			// Capture input
			ImGuiIO& io = ImGui::GetIO();
			for (int key = 0; key < 512; key++) {
				if (ImGui::IsKeyPressed((ImGuiKey)key, false)) {
					// Convert ImGui key to KeyCode (simplified - needs proper mapping)
					m_CapturedKey = static_cast<KeyCode>(key);

					// Capture modifiers
					m_CapturedModifiers = KeyModifiers::None;
					if (io.KeyCtrl)  m_CapturedModifiers |= KeyModifiers::Ctrl;
					if (io.KeyShift) m_CapturedModifiers |= KeyModifiers::Shift;
					if (io.KeyAlt)   m_CapturedModifiers |= KeyModifiers::Alt;
					if (io.KeySuper) m_CapturedModifiers |= KeyModifiers::Super;

					break;
				}
			}

			ImGui::EndPopup();
		}
	}

	void InputSettingsPanel::RenderConfirmDialog() {
		if (!m_ShowConfirmReset)
			return;

		ImGui::OpenPopup("Confirm Reset");

		ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Confirm Reset", nullptr, ImGuiWindowFlags_NoResize)) {
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_WARNING);
			ImGui::TextWrapped("?? Are you sure you want to reset all bindings to defaults?");
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Text("This action cannot be undone.");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_DANGER);
			if (ImGui::Button("Yes, Reset", ImVec2(120, 0))) {
				ResetToDefaults();
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_ShowConfirmReset = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void InputSettingsPanel::BeginRemap(const std::string& actionName) {
		m_IsRemapping = true;
		m_RemapActionName = actionName;
		m_CapturedKey = Key::Space; // Changed from Key::Unknown
		m_CapturedModifiers = KeyModifiers::None;
	}

	void InputSettingsPanel::CancelRemap() {
		m_IsRemapping = false;
		m_RemapActionName.clear();
		m_CapturedKey = Key::Space; // Changed from Key::Unknown
		m_CapturedModifiers = KeyModifiers::None;
	}

	void InputSettingsPanel::ApplyRemap() {
		auto& keyMap = InputManager::Get().GetKeyMap();

		// Remove existing binding for this action
		keyMap.UnbindAction(m_RemapActionName);

		// Add new binding
		keyMap.Bind(m_CapturedKey, m_CapturedModifiers, m_RemapActionName);

		LNX_LOG_INFO("Remapped '{0}' to {1}", m_RemapActionName, 
			KeyBinding(m_CapturedKey, m_CapturedModifiers, "").ToString());

		CancelRemap();
	}

	void InputSettingsPanel::ResetToDefaults() {
		InputManager::Get().ResetToDefaults();
		LNX_LOG_INFO("Reset input bindings to defaults");
	}

	void InputSettingsPanel::SaveBindings() {
		// Save to project
		if (InputManager::Get().SaveBindingsToProject()) {
			LNX_LOG_INFO("Saved input bindings to project");
		}
	}

	void InputSettingsPanel::LoadBindings() {
		// Load from project
		if (InputManager::Get().LoadBindingsFromProject()) {
			LNX_LOG_INFO("Loaded input bindings from project");
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
