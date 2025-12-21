#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Scene/Core/SceneMode.h"  // AAA Architecture: Use SceneMode
#include <functional>
#include <glm/glm.hpp>

namespace Lunex {
	// Legacy alias for backwards compatibility
	using SceneState = SceneMode;

	class ToolbarPanel {
	public:
		ToolbarPanel() = default;
		~ToolbarPanel() = default;

		// Renderiza la toolbar flotante sobre el viewport
		void OnImGuiRender(SceneMode sceneState, bool toolbarEnabled,
						   const glm::vec2& viewportBounds, const glm::vec2& viewportSize);

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
		Ref<Texture2D> m_IconPlay;
		Ref<Texture2D> m_IconSimulate;
		Ref<Texture2D> m_IconStop;
		Ref<Texture2D> m_IconPause;

		std::function<void()> m_OnPlayCallback;
		std::function<void()> m_OnSimulateCallback;
		std::function<void()> m_OnStopCallback;
		std::function<void()> m_OnPauseCallback;
	};
}
