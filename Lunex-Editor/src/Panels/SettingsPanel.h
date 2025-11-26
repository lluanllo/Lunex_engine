#pragma once

#include "Renderer/SkyboxRenderer.h"

namespace Lunex {
	class SettingsPanel {
	public:
		SettingsPanel() = default;
		~SettingsPanel() = default;

		void OnImGuiRender();

		bool GetShowPhysicsColliders() const { return m_ShowPhysicsColliders; }
		void SetShowPhysicsColliders(bool show) { m_ShowPhysicsColliders = show; }
		
		// Skybox settings
		SkyboxSettings& GetSkyboxSettings() { return m_SkyboxSettings; }
		const SkyboxSettings& GetSkyboxSettings() const { return m_SkyboxSettings; }

	private:
		bool m_ShowPhysicsColliders = false;
		SkyboxSettings m_SkyboxSettings;
	};
}
