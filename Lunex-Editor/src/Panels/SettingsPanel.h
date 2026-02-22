#pragma once

#include "../UI/LunexUI.h"
#include "Project/Project.h"
#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace Lunex {
	class SettingsPanel {
	public:
		SettingsPanel() = default;
		~SettingsPanel() = default;

		void OnImGuiRender();

		bool GetShowPhysicsColliders() const { return m_ShowPhysicsColliders; }
		void SetShowPhysicsColliders(bool show) { m_ShowPhysicsColliders = show; }

		bool GetShowPhysics3DColliders() const { return m_ShowPhysics3DColliders; }
		void SetShowPhysics3DColliders(bool show) { m_ShowPhysics3DColliders = show; }

		// Post-Processing config Load/Save from project
		void LoadPostProcessFromConfig(const PostProcessPreferencesConfig& config);
		void SavePostProcessToConfig(PostProcessPreferencesConfig& config) const;

		// Callback invoked whenever any post-process setting changes (for autosave)
		void SetOnPostProcessChangedCallback(const std::function<void()>& callback) { m_OnPostProcessChanged = callback; }

	private:
		// Tab drawing helpers
		void DrawRenderTab();
		void DrawEnvironmentTab();
		void DrawShadowsTab();
		void DrawPhysicsTab();
		
		// Section drawing helpers
		void DrawRenderSection();
		void DrawPostProcessSection();
		void DrawEnvironmentSection();
		void DrawShadowsSection();
		void DrawPhysics2DSection();
		void DrawPhysics3DSection();
		void DrawPhysicsGeneralSection();
		
		void NotifyPostProcessChanged();
		
	private:
		bool m_ShowPhysicsColliders = false;
		bool m_ShowPhysics3DColliders = false;
		
		// Skybox settings cache for UI
		std::string m_HDRIPath;

		// Autosave callback
		std::function<void()> m_OnPostProcessChanged;
	};
}