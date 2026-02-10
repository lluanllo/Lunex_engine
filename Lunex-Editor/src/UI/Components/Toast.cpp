/**
 * @file Toast.cpp
 * @brief Toast Notification Component Implementation
 */

#include "stpch.h"
#include "Toast.h"

namespace Lunex::UI {

	ToastManager& ToastManager::Get() {
		static ToastManager instance;
		return instance;
	}
	
	void ToastManager::Show(const std::string& message, ToastType type, float duration) {
		ToastNotification toast;
		toast.message = message;
		toast.type = type;
		toast.duration = duration;
		toast.elapsed = 0.0f;
		toast.alpha = 1.0f;
		
		m_ActiveToasts.push_back(toast);
	}
	
	void ToastManager::Render() {
		if (m_ActiveToasts.empty()) return;
		
		ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
		float yOffset = m_Style.padding;
		float deltaTime = ImGui::GetIO().DeltaTime;
		
		for (auto it = m_ActiveToasts.begin(); it != m_ActiveToasts.end();) {
			auto& toast = *it;
			
			UpdateToast(toast, deltaTime);
			
			// Remove expired toasts
			if (toast.elapsed >= toast.duration) {
				it = m_ActiveToasts.erase(it);
				continue;
			}
			
			int index = (int)std::distance(m_ActiveToasts.begin(), it);
			RenderToast(toast, index, yOffset);
			
			yOffset += m_Style.height + SpacingValues::SM;
			++it;
		}
	}
	
	void ToastManager::Clear() {
		m_ActiveToasts.clear();
	}
	
	void ToastManager::UpdateToast(ToastNotification& toast, float deltaTime) {
		toast.elapsed += deltaTime;
		
		// Calculate alpha for fade in/out
		if (toast.elapsed < m_Style.fadeTime) {
			toast.alpha = toast.elapsed / m_Style.fadeTime;
		} else if (toast.elapsed > toast.duration - m_Style.fadeTime) {
			toast.alpha = (toast.duration - toast.elapsed) / m_Style.fadeTime;
		} else {
			toast.alpha = 1.0f;
		}
	}
	
	void ToastManager::RenderToast(const ToastNotification& toast, int index, float yOffset) {
		ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
		
		// Position at bottom-right
		ImVec2 pos = ImVec2(
			viewportSize.x - m_Style.width - m_Style.padding,
			viewportSize.y - yOffset - m_Style.height
		);
		
		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(ImVec2(m_Style.width, m_Style.height));
		
		std::string windowName = "##Toast" + std::to_string(index);
		
		Color bgColor = GetToastColor(toast.type, toast.alpha);
		
		ScopedColor colors({
			{ImGuiCol_WindowBg, bgColor},
			{ImGuiCol_Border, Color(0, 0, 0, 0)}
		});
		ScopedStyle vars(
			ImGuiStyleVar_WindowRounding, m_Style.rounding,
			ImGuiStyleVar_WindowBorderSize, 0.0f
		);
		
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
								 ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoNav |
								 ImGuiWindowFlags_NoFocusOnAppearing |
								 ImGuiWindowFlags_NoDocking |
								 ImGuiWindowFlags_NoSavedSettings;
		
		if (ImGui::Begin(windowName.c_str(), nullptr, flags)) {
			Color textColor = Colors::TextPrimary();
			textColor.a = toast.alpha;
			
			ScopedColor tc(ImGuiCol_Text, textColor);
			ImGui::Text("%s %s", GetToastIcon(toast.type), toast.message.c_str());
		}
		ImGui::End();
	}
	
	Color ToastManager::GetToastColor(ToastType type, float alpha) const {
		switch (type) {
			case ToastType::Success:
				return Color(0.10f, 0.30f, 0.15f, alpha * 0.95f);
			case ToastType::Warning:
				return Color(0.35f, 0.25f, 0.08f, alpha * 0.95f);
			case ToastType::Error:
				return Color(0.35f, 0.10f, 0.10f, alpha * 0.95f);
			default:
				return Color(0.10f, 0.10f, 0.18f, alpha * 0.95f);
		}
	}
	
	const char* ToastManager::GetToastIcon(ToastType type) const {
		switch (type) {
			case ToastType::Success: return "?";
			case ToastType::Warning: return "?";
			case ToastType::Error:   return "?";
			default:                 return "?";
		}
	}
	
	// ============================================================================
	// GLOBAL HELPER FUNCTIONS
	// ============================================================================
	
	void ShowToast(const std::string& message, ToastType type, float duration) {
		ToastManager::Get().Show(message, type, duration);
	}
	
	void RenderToasts() {
		ToastManager::Get().Render();
	}

} // namespace Lunex::UI
