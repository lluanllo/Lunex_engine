/**
 * @file Dialog.cpp
 * @brief Dialog Components Implementation
 */

#include "stpch.h"
#include "Dialog.h"

namespace Lunex::UI {

	// ============================================================================
	// INPUT DIALOG
	// ============================================================================
	
	void InputDialog::Open(const std::string& title, const std::string& label,
						   const std::string& defaultValue) {
		m_Title = title;
		m_Label = label;
		strncpy_s(m_Buffer, defaultValue.c_str(), sizeof(m_Buffer) - 1);
		m_FirstFrame = true;
		m_IsOpen = true;
		ImGui::OpenPopup(m_Title.c_str());
	}
	
	InputDialogResult InputDialog::Render(const std::string& confirmLabel,
										  const std::string& cancelLabel) {
		InputDialogResult result;
		
		if (!m_IsOpen) return result;
		
		CenterNextWindow();
		
		if (BeginModal(m_Title, nullptr, Size(400, 150))) {
			if (m_FirstFrame) {
				ImGui::SetKeyboardFocusHere();
				m_FirstFrame = false;
			}
			
			TextStyled(m_Label);
			AddSpacing(SpacingValues::SM);
			
			ImGui::SetNextItemWidth(-1);
			bool enterPressed = ImGui::InputText("##input", m_Buffer, sizeof(m_Buffer), 
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			
			AddSpacing(SpacingValues::MD);
			
			float buttonWidth = 100.0f;
			float spacing = 10.0f;
			float totalWidth = buttonWidth * 2 + spacing;
			float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
			
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			
			if (Button(confirmLabel, ButtonVariant::Primary, ButtonSize::Medium, Size(buttonWidth, 0)) || enterPressed) {
				result.confirmed = true;
				result.value = m_Buffer;
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			
			SameLine(0, spacing);
			
			if (Button(cancelLabel, ButtonVariant::Default, ButtonSize::Medium, Size(buttonWidth, 0))) {
				result.cancelled = true;
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			
			EndModal();
		}
		
		return result;
	}
	
	// ============================================================================
	// CONFIRM DIALOG
	// ============================================================================
	
	void ConfirmDialog::Open(const std::string& title, const std::string& message,
							 bool isDangerous) {
		m_Title = title;
		m_Message = message;
		m_IsDangerous = isDangerous;
		m_IsOpen = true;
		ImGui::OpenPopup(m_Title.c_str());
	}
	
	ConfirmDialogResult ConfirmDialog::Render(const std::string& confirmLabel,
											  const std::string& cancelLabel) {
		ConfirmDialogResult result;
		
		if (!m_IsOpen) return result;
		
		CenterNextWindow();
		
		if (BeginModal(m_Title, nullptr, Size(400, 150))) {
			TextWrapped(m_Message, TextVariant::Default);
			
			AddSpacing(SpacingValues::LG);
			
			float buttonWidth = 100.0f;
			float spacing = 10.0f;
			float totalWidth = buttonWidth * 2 + spacing;
			float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
			
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			
			ButtonVariant confirmVariant = m_IsDangerous ? ButtonVariant::Danger : ButtonVariant::Primary;
			
			if (Button(confirmLabel, confirmVariant, ButtonSize::Medium, Size(buttonWidth, 0))) {
				result.confirmed = true;
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			
			SameLine(0, spacing);
			
			if (Button(cancelLabel, ButtonVariant::Default, ButtonSize::Medium, Size(buttonWidth, 0))) {
				result.cancelled = true;
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			
			EndModal();
		}
		
		return result;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	static InputDialog s_InputDialog;
	static ConfirmDialog s_ConfirmDialog;
	static bool s_InputDialogInitialized = false;
	static bool s_ConfirmDialogInitialized = false;
	
	InputDialogResult ShowInputDialog(const std::string& title, const std::string& label,
									  const std::string& defaultValue,
									  const std::string& confirmLabel,
									  const std::string& cancelLabel) {
		if (!s_InputDialogInitialized) {
			s_InputDialog.Open(title, label, defaultValue);
			s_InputDialogInitialized = true;
		}
		
		auto result = s_InputDialog.Render(confirmLabel, cancelLabel);
		
		if (result.confirmed || result.cancelled) {
			s_InputDialogInitialized = false;
		}
		
		return result;
	}
	
	ConfirmDialogResult ShowConfirmDialog(const std::string& title, const std::string& message,
										  const std::string& confirmLabel,
										  const std::string& cancelLabel, bool isDangerous) {
		if (!s_ConfirmDialogInitialized) {
			s_ConfirmDialog.Open(title, message, isDangerous);
			s_ConfirmDialogInitialized = true;
		}
		
		auto result = s_ConfirmDialog.Render(confirmLabel, cancelLabel);
		
		if (result.confirmed || result.cancelled) {
			s_ConfirmDialogInitialized = false;
		}
		
		return result;
	}

} // namespace Lunex::UI
