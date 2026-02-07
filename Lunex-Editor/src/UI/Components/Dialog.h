/**
 * @file Dialog.h
 * @brief Dialog Components (Input, Confirm, etc.)
 */

#pragma once

#include "../UICore.h"
#include "../UIComponents.h"
#include "../UILayout.h"

namespace Lunex::UI {

	// ============================================================================
	// DIALOG RESULTS
	// ============================================================================
	
	struct InputDialogResult {
		bool confirmed = false;
		bool cancelled = false;
		std::string value;
	};
	
	struct ConfirmDialogResult {
		bool confirmed = false;
		bool cancelled = false;
	};
	
	// ============================================================================
	// INPUT DIALOG COMPONENT
	// ============================================================================
	
	/**
	 * @class InputDialog
	 * @brief Modal dialog for text input
	 * 
	 * Features:
	 * - Text input field
	 * - OK/Cancel buttons
	 * - Enter key support
	 * - Auto-select on open
	 */
	class InputDialog {
	public:
		InputDialog() = default;
		~InputDialog() = default;
		
		/**
		 * @brief Open the dialog
		 * @param title Dialog title
		 * @param label Input label
		 * @param defaultValue Default input value
		 */
		void Open(const std::string& title,
				  const std::string& label,
				  const std::string& defaultValue = "");
		
		/**
		 * @brief Render the dialog
		 * @param confirmLabel Confirm button label
		 * @param cancelLabel Cancel button label
		 * @return Dialog result
		 */
		InputDialogResult Render(const std::string& confirmLabel = "OK",
								 const std::string& cancelLabel = "Cancel");
		
		/**
		 * @brief Check if dialog is open
		 */
		bool IsOpen() const { return m_IsOpen; }
		
	private:
		bool m_IsOpen = false;
		bool m_FirstFrame = true;
		std::string m_Title;
		std::string m_Label;
		char m_Buffer[256] = {};
	};
	
	// ============================================================================
	// CONFIRM DIALOG COMPONENT
	// ============================================================================
	
	/**
	 * @class ConfirmDialog
	 * @brief Modal dialog for confirmation
	 * 
	 * Features:
	 * - Message display
	 * - OK/Cancel buttons
	 * - Danger mode styling
	 */
	class ConfirmDialog {
	public:
		ConfirmDialog() = default;
		~ConfirmDialog() = default;
		
		/**
		 * @brief Open the dialog
		 * @param title Dialog title
		 * @param message Confirmation message
		 * @param isDangerous Whether to style as dangerous action
		 */
		void Open(const std::string& title,
				  const std::string& message,
				  bool isDangerous = false);
		
		/**
		 * @brief Render the dialog
		 * @param confirmLabel Confirm button label
		 * @param cancelLabel Cancel button label
		 * @return Dialog result
		 */
		ConfirmDialogResult Render(const std::string& confirmLabel = "Confirm",
								   const std::string& cancelLabel = "Cancel");
		
		/**
		 * @brief Check if dialog is open
		 */
		bool IsOpen() const { return m_IsOpen; }
		
	private:
		bool m_IsOpen = false;
		std::string m_Title;
		std::string m_Message;
		bool m_IsDangerous = false;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS (One-shot dialogs)
	// ============================================================================
	
	/**
	 * @brief Show an input dialog
	 * @note Must be called every frame while open
	 */
	InputDialogResult ShowInputDialog(const std::string& title,
									  const std::string& label,
									  const std::string& defaultValue = "",
									  const std::string& confirmLabel = "OK",
									  const std::string& cancelLabel = "Cancel");
	
	/**
	 * @brief Show a confirm dialog
	 * @note Must be called every frame while open
	 */
	ConfirmDialogResult ShowConfirmDialog(const std::string& title,
										  const std::string& message,
										  const std::string& confirmLabel = "Confirm",
										  const std::string& cancelLabel = "Cancel",
										  bool isDangerous = false);

} // namespace Lunex::UI
