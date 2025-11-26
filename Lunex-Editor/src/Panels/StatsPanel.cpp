#include "StatsPanel.h"
#include "imgui.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RayTracingManager.h"

namespace Lunex {
	void StatsPanel::OnImGuiRender() {
		ImGui::Begin("Stats");

		std::string name = "None";
		if (m_HoveredEntity)
			name = m_HoveredEntity.GetComponent<TagComponent>().Tag;
		ImGui::Text("Hovered Entity: %s", name.c_str());

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto stats2D = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats2D.DrawCalls);
		ImGui::Text("Quads: %d", stats2D.QuadCount);
		ImGui::Text("Vertices: %d", stats2D.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats2D.GetTotalIndexCount());

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto stats3D = Renderer3D::GetStats();
		ImGui::Text("Renderer3D Stats:");
		ImGui::Text("Draw Calls: %d", stats3D.DrawCalls);
		ImGui::Text("Meshes: %d", stats3D.MeshCount);
		ImGui::Text("Triangles: %d", stats3D.TriangleCount);

		// Ray Tracing Stats
		if (m_RayTracingManager) {
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			auto rtStats = m_RayTracingManager->GetStats();
			ImGui::Text("Ray Tracing Stats:");
			ImGui::Text("Scene Triangles: %u", rtStats.triangleCount);
			ImGui::Text("BVH Nodes: %u", rtStats.bvhNodeCount);
			ImGui::Text("BVH Build: %.2f ms", rtStats.bvhBuildTime);
			ImGui::Text("Shadow Compute: %.2f ms", rtStats.shadowComputeTime);
			
			if (rtStats.geometryDirty) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Geometry: DIRTY");
			} else {
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Geometry: UP TO DATE");
			}
		}

		ImGui::End();
	}
}
