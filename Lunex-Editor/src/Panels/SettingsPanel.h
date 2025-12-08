#pragma once

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

	private:
		bool m_ShowPhysicsColliders = false;
		bool m_ShowPhysics3DColliders = false;
		
		// Skybox settings cache for UI
		std::string m_HDRIPath;
	};
}
