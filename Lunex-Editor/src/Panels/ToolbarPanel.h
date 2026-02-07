/**
 * @file ToolbarPanel.h
 * @brief Toolbar Panel - Floating Play/Stop/Simulate toolbar
 * 
 * Features:
 * - Floating toolbar positioned above viewport
 * - Play, Stop, Simulate buttons with icons
 * - Translucent style matching editor theme
 * - State-aware button display
 */

#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Scene/Core/SceneMode.h"

#include <functional>
#include <glm/glm.hpp>

namespace Lunex {
	// Legacy alias for backwards compatibility
	using SceneState = SceneMode;

	class ToolbarPanel {
	public:
		ToolbarPanel() = default;
		~ToolbarPanel() = default;

		void OnImGuiRender(SceneMode sceneState, bool toolbarEnabled,
						   const glm::vec2& viewportBounds, const glm::vec2& viewportSize);

		// Icon setters
		void SetPlayIcon(Ref<Texture2D> icon) { m_IconPlay = icon; }
		void SetSimulateIcon(Ref<Texture2D> icon) { m_IconSimulate = icon; }
		void SetStopIcon(Ref<Texture2D> icon) { m_IconStop = icon; }
		void SetPauseIcon(Ref<Texture2D> icon) { m_IconPause = icon; }

		// Callbacks
		void SetOnPlayCallback(std::function<void()> callback) { m_OnPlayCallback = callback; }
		void SetOnSimulateCallback(std::function<void()> callback) { m_OnSimulateCallback = callback; }
		void SetOnStopCallback(std::function<void()> callback) { m_OnStopCallback = callback; }
		void SetOnPauseCallback(std::function<void()> callback) { m_OnPauseCallback = callback; }

	private:
		// Render helpers
		void RenderPlayStopButton(SceneMode sceneState, bool enabled);
		void RenderSimulateButton(SceneMode sceneState, bool enabled);

	private:
		// Icons
		Ref<Texture2D> m_IconPlay;
		Ref<Texture2D> m_IconSimulate;
		Ref<Texture2D> m_IconStop;
		Ref<Texture2D> m_IconPause;

		// Callbacks
		std::function<void()> m_OnPlayCallback;
		std::function<void()> m_OnSimulateCallback;
		std::function<void()> m_OnStopCallback;
		std::function<void()> m_OnPauseCallback;
	};

} // namespace Lunex
