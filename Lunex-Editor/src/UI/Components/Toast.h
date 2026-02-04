/**
 * @file Toast.h
 * @brief Toast Notification Component
 */

#pragma once

#include "../UICore.h"
#include <deque>

namespace Lunex::UI {

	// ============================================================================
	// TOAST NOTIFICATION COMPONENT
	// ============================================================================
	
	enum class ToastType {
		Info,
		Success,
		Warning,
		Error
	};
	
	struct ToastStyle {
		float width = 300.0f;
		float height = 50.0f;
		float padding = 20.0f;
		float fadeTime = 0.3f;
		float rounding = 6.0f;
	};
	
	/**
	 * @class ToastManager
	 * @brief Manages and renders toast notifications
	 * 
	 * Features:
	 * - Multiple concurrent toasts
	 * - Fade in/out animation
	 * - Different types (Info, Success, Warning, Error)
	 * - Auto-dismiss with configurable duration
	 */
	class ToastManager {
	public:
		static ToastManager& Get();
		
		/**
		 * @brief Show a toast notification
		 * @param message Message to display
		 * @param type Toast type
		 * @param duration Duration in seconds
		 */
		void Show(const std::string& message,
				  ToastType type = ToastType::Info,
				  float duration = 3.0f);
		
		/**
		 * @brief Render all active toasts
		 * Call once per frame
		 */
		void Render();
		
		/**
		 * @brief Clear all toasts
		 */
		void Clear();
		
		// Style configuration
		void SetStyle(const ToastStyle& style) { m_Style = style; }
		ToastStyle& GetStyle() { return m_Style; }
		const ToastStyle& GetStyle() const { return m_Style; }
		
	private:
		ToastManager() = default;
		
		struct ToastNotification {
			std::string message;
			ToastType type;
			float duration;
			float elapsed;
			float alpha;
		};
		
		ToastStyle m_Style;
		std::deque<ToastNotification> m_ActiveToasts;
		
		void UpdateToast(ToastNotification& toast, float deltaTime);
		void RenderToast(const ToastNotification& toast, int index, float yOffset);
		Color GetToastColor(ToastType type, float alpha) const;
		const char* GetToastIcon(ToastType type) const;
	};
	
	// ============================================================================
	// GLOBAL HELPER FUNCTIONS
	// ============================================================================
	
	/**
	 * @brief Show a toast notification
	 */
	void ShowToast(const std::string& message,
				   ToastType type = ToastType::Info,
				   float duration = 3.0f);
	
	/**
	 * @brief Render all active toasts
	 */
	void RenderToasts();

} // namespace Lunex::UI
