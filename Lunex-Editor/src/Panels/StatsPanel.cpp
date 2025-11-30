#include "StatsPanel.h"
#include "imgui.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"

namespace Lunex {
	void StatsPanel::OnImGuiRender() {
		if (!m_IsOpen)
			return;
			
		ImGui::Begin("Stats", &m_IsOpen);

		std::string name = "None";
		if (m_HoveredEntity)
			name = m_HoveredEntity.GetComponent<TagComponent>().Tag;
		ImGui::Text("Hovered Entity: %s", name.c_str());

		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

		ImGui::End();
	}
}
