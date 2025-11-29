#include "SettingsPanel.h"
#include "imgui.h"

namespace Lunex {
	void SettingsPanel::OnImGuiRender() {
		ImGui::Begin("Settings");
		
		// Physics 2D Colliders (red)
		ImGui::Text("Physics 2D");
		ImGui::Checkbox("Show 2D colliders", &m_ShowPhysicsColliders);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Display Box2D colliders in red");
		}
		
		ImGui::Separator();
		
		// Physics 3D Colliders (green)
		ImGui::Text("Physics 3D");
		ImGui::Checkbox("Show 3D colliders", &m_ShowPhysics3DColliders);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Display Bullet3D colliders in green");
		}
		
		ImGui::End();
	}
}
