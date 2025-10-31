#pragma once

namespace Lunex {
	class SettingsPanel {
	public:
		SettingsPanel() = default;
		~SettingsPanel() = default;

		void OnImGuiRender();

		bool GetShowPhysicsColliders() const { return m_ShowPhysicsColliders; }
		void SetShowPhysicsColliders(bool show) { m_ShowPhysicsColliders = show; }

	private:
		bool m_ShowPhysicsColliders = false;
	};
}
