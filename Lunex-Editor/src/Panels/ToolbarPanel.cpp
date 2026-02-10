/**
 * @file ToolbarPanel.cpp
 * @brief Toolbar Panel - Floating Play/Stop/Simulate toolbar
 * 
 * Features:
 * - Floating toolbar positioned above viewport
 * - Play, Stop, Simulate buttons with icons
 * - Translucent style matching editor theme
 * - State-aware button display
 */

#include "ToolbarPanel.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	// ============================================================================
	// TOOLBAR CONSTANTS
	// ============================================================================
	
	namespace {
		constexpr float kButtonSize = 32.0f;
		constexpr float kSpacing = 8.0f;
		constexpr float kPadding = 32.0f;
		constexpr int kButtonCount = 2;  // Play/Stop + Simulate
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void ToolbarPanel::OnImGuiRender(SceneMode sceneState, bool toolbarEnabled,
									 const glm::vec2& viewportBounds, const glm::vec2& viewportSize) {
		using namespace UI;
		
		// Skip if viewport too small
		if (viewportSize.x < 100 || viewportSize.y < 100) return;
		
		// Calculate dimensions
		float totalWidth = (kButtonSize * kButtonCount) + (kSpacing * (kButtonCount - 1)) + (kPadding * 2);
		float totalHeight = kButtonSize + (kPadding * 2);
		
		// Position: centered horizontally, top of viewport
		float toolbarX = viewportBounds.x + (viewportSize.x * 0.5f) - (totalWidth * 0.5f);
		float toolbarY = viewportBounds.y - 20.0f;
		
		ImGui::SetNextWindowPos(ImVec2(toolbarX, toolbarY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
		
		// Transparent container style
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPadding, kPadding));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kSpacing, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		
		ImGuiWindowFlags windowFlags = 
			ImGuiWindowFlags_NoDecoration | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoDocking;
		
		ImGui::Begin("##FloatingToolbar", nullptr, windowFlags);
		
		// Play/Stop Button
		RenderPlayStopButton(sceneState, toolbarEnabled);
		
		SameLine();
		
		// Simulate Button
		RenderSimulateButton(sceneState, toolbarEnabled);
		
		ImGui::End();
		
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}

	// ============================================================================
	// PLAY/STOP BUTTON
	// ============================================================================

	void ToolbarPanel::RenderPlayStopButton(SceneMode sceneState, bool enabled) {
		using namespace UI;
		
		bool isPlaying = (sceneState == SceneState::Play);
		Ref<Texture2D> icon = isPlaying ? m_IconStop : m_IconPlay;
		const char* fallback = isPlaying ? "■" : "▶";
		const char* tooltip = isPlaying ? "Stop (Ctrl+P)" : "Play (Ctrl+P)";
		
		ToolbarButtonProps props;
		props.Id = "PlayButton";
		props.Icon = icon;
		props.FallbackText = fallback;
		props.Size = kButtonSize;
		props.IsEnabled = enabled;
		props.TooltipTitle = tooltip;
		props.Tint = enabled ? Colors::TextPrimary() : Colors::TextMuted();
		
		if (ToolbarButton(props)) {
			if (sceneState == SceneState::Edit || sceneState == SceneState::Simulate) {
				LNX_LOG_INFO("Play button clicked");
				if (m_OnPlayCallback) m_OnPlayCallback();
			}
			else if (sceneState == SceneState::Play) {
				LNX_LOG_INFO("Stop button clicked");
				if (m_OnStopCallback) m_OnStopCallback();
			}
		}
	}

	// ============================================================================
	// SIMULATE BUTTON
	// ============================================================================

	void ToolbarPanel::RenderSimulateButton(SceneMode sceneState, bool enabled) {
		using namespace UI;
		
		bool isSimulating = (sceneState == SceneState::Simulate);
		Ref<Texture2D> icon = isSimulating ? m_IconStop : m_IconSimulate;
		const char* fallback = isSimulating ? "■" : "🔁";
		const char* tooltip = isSimulating ? "Stop Simulation" : "Simulate Physics";
		
		ToolbarButtonProps props;
		props.Id = "SimulateButton";
		props.Icon = icon;
		props.FallbackText = fallback;
		props.Size = kButtonSize;
		props.IsEnabled = enabled;
		props.TooltipTitle = tooltip;
		props.Tint = enabled ? Colors::TextPrimary() : Colors::TextMuted();
		
		if (ToolbarButton(props)) {
			if (sceneState == SceneState::Edit || sceneState == SceneState::Play) {
				LNX_LOG_INFO("Simulate button clicked");
				if (m_OnSimulateCallback) m_OnSimulateCallback();
			}
			else if (sceneState == SceneState::Simulate) {
				LNX_LOG_INFO("Stop simulate button clicked");
				if (m_OnStopCallback) m_OnStopCallback();
			}
		}
	}

} // namespace Lunex
