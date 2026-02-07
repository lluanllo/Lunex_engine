/**
 * @file StatsPanel.cpp
 * @brief Stats Panel - Display renderer and scene statistics
 * 
 * Features:
 * - Renderer2D statistics (draw calls, quads, vertices, indices)
 * - Renderer3D statistics (meshes, triangles)
 * - Hovered entity information
 * - Clean, organized display
 */

#include "StatsPanel.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	void StatsPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		using namespace UI;

		if (BeginPanel("Stats", &m_IsOpen)) {
			// Hovered Entity Section
			{
				std::string entityName = m_HoveredEntity 
					? m_HoveredEntity.GetComponent<TagComponent>().Tag 
					: "None";
				
				StatItem("Hovered Entity", entityName.c_str());
				AddSpacing(SpacingValues::MD);
				Separator();
				AddSpacing(SpacingValues::MD);
			}

			// Renderer2D Stats
			{
				auto stats2D = Renderer2D::GetStats();
				
				StatHeader("Renderer2D Stats");
				AddSpacing(SpacingValues::SM);
				
				StatItem("Draw Calls", stats2D.DrawCalls);
				StatItem("Quads", stats2D.QuadCount);
				StatItem("Vertices", stats2D.GetTotalVertexCount());
				StatItem("Indices", stats2D.GetTotalIndexCount());
				
				AddSpacing(SpacingValues::MD);
				Separator();
				AddSpacing(SpacingValues::MD);
			}

			// Renderer3D Stats
			{
				auto stats3D = Renderer3D::GetStats();
				
				StatHeader("Renderer3D Stats");
				AddSpacing(SpacingValues::SM);
				
				StatItem("Draw Calls", stats3D.DrawCalls);
				StatItem("Meshes", stats3D.MeshCount);
				StatItem("Triangles", stats3D.TriangleCount);
			}
		}
		EndPanel();
	}

} // namespace Lunex
