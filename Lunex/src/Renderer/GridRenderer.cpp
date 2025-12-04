#include "stpch.h"
#include "GridRenderer.h"

#include "Renderer/Renderer2D.h"

namespace Lunex {

	GridRenderer::GridSettings GridRenderer::s_Settings;

	void GridRenderer::Init() {
		LNX_PROFILE_FUNCTION();
	}

	void GridRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
	}

	void GridRenderer::DrawGrid(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();

		// Grid parameters
		const float gridExtent = s_Settings.FadeDistance;  // How far the grid extends from center
		const float cellSize = s_Settings.GridScale;       // Size of each cell (1.0 = 1 unit)
		const int numCells = static_cast<int>(gridExtent / cellSize);
		
		// Grid colors - subtle and thin
		glm::vec4 lineColor = glm::vec4(s_Settings.GridColor, 0.25f);
		glm::vec4 majorColor = glm::vec4(s_Settings.GridColor * 0.6f, 0.4f);
		
		// Set thin line width
		float previousLineWidth = Renderer2D::GetLineWidth();
		Renderer2D::SetLineWidth(s_Settings.MinorLineThickness);
		
		// Draw grid lines on XZ plane (Y = 0)
		// Lines parallel to Z axis
		for (int i = -numCells; i <= numCells; i++) {
			float x = i * cellSize;
			
			// Every 10 lines is a major line (but not the center)
			bool isMajor = (i % 10 == 0) && (i != 0);
			glm::vec4 color = isMajor ? majorColor : lineColor;
			
			glm::vec3 start = { x, 0.0f, -gridExtent };
			glm::vec3 end = { x, 0.0f, gridExtent };
			
			Renderer2D::DrawLine(start, end, color);
		}
		
		// Lines parallel to X axis
		for (int i = -numCells; i <= numCells; i++) {
			float z = i * cellSize;
			
			bool isMajor = (i % 10 == 0) && (i != 0);
			glm::vec4 color = isMajor ? majorColor : lineColor;
			
			glm::vec3 start = { -gridExtent, 0.0f, z };
			glm::vec3 end = { gridExtent, 0.0f, z };
			
			Renderer2D::DrawLine(start, end, color);
		}
		
		// Restore previous line width
		Renderer2D::SetLineWidth(previousLineWidth);
	}

}
