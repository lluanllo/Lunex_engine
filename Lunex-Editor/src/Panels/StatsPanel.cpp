#include "StatsPanel.h"
#include "imgui.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"

namespace Lunex {
	void StatsPanel::OnImGuiRender() {
		ImGui::Begin("Stats");

		std::string name = "None";
		// Check if entity is valid before accessing components
		if (m_HoveredEntity && m_HoveredEntity.HasComponent<TagComponent>())
			name = m_HoveredEntity.GetComponent<TagComponent>().Tag;
		ImGui::Text("Hovered Entity: %s", name.c_str());

		ImGui::Separator();
		
		auto stats2D = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats2D.DrawCalls);
		ImGui::Text("Quads: %d", stats2D.QuadCount);
		ImGui::Text("Vertices: %d", stats2D.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats2D.GetTotalIndexCount());

		ImGui::Separator();
		
		auto stats3D = Renderer3D::GetStats();
		ImGui::Text("Renderer3D Stats:");
		ImGui::Text("Draw Calls: %d", stats3D.DrawCalls);
		ImGui::Text("Meshes: %d", stats3D.MeshCount);
		ImGui::Text("Triangles: %d", stats3D.TriangleCount);

		ImGui::End();
	}
}
