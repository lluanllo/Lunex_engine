#pragma once

#include "../UI/LunexUI.h"
#include "Rendering/RenderBackend.h"
#include "Scene/Systems/SceneRenderSystem.h"
#include <string>
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

		/** Must be called so the Render tab can access the backend. */
		void SetSceneRenderSystem(SceneRenderSystem* sys) { m_RenderSystem = sys; }

	private:
		// Tab drawing helpers
		void DrawRenderTab();
		void DrawEnvironmentTab();
		void DrawShadowsTab();
		void DrawPhysicsTab();
		
	private:
		bool m_ShowPhysicsColliders = false;
		bool m_ShowPhysics3DColliders = false;

		// Skybox settings cache for UI
		std::string m_HDRIPath;

		// Reference to the render system for backend switching
		SceneRenderSystem* m_RenderSystem = nullptr;
	};
}