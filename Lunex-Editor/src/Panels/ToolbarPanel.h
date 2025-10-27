#pragma once
#include "Core/Core.h"
#include "Renderer/Texture.h"
#include <functional>

namespace Lunex {
	enum class SceneState {
		Edit = 0,
		Play = 1,
		Simulate = 2
	};
	
	struct ToolbarCallbacks {
		std::function<void()> OnPlay;
		std::function<void()> OnSimulate;
		std::function<void()> OnStop;
	};
	
	class ToolbarPanel {
		public:
			ToolbarPanel();
			~ToolbarPanel() = default;
			
			void OnImGuiRender();
			
			void SetSceneState(SceneState state) { m_SceneState = state; }
			void SetToolbarEnabled(bool enabled) { m_ToolbarEnabled = enabled; }
			void SetCallbacks(const ToolbarCallbacks& callbacks) { m_Callbacks = callbacks; }
			
		private:
			Ref<Texture2D> m_IconPlay;
			Ref<Texture2D> m_IconSimulate;
			Ref<Texture2D> m_IconStop;
			
			SceneState m_SceneState = SceneState::Edit;
			bool m_ToolbarEnabled = true;
			ToolbarCallbacks m_Callbacks;
	};
}