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
		
		float previousLineWidth = Renderer2D::GetLineWidth();

		// Two passes so minor and major lines can have different thickness
		for (int pass = 0; pass < 2; pass++) {
			const bool majorPass = (pass == 1);
			Renderer2D::SetLineWidth(majorPass ? s_Settings.MajorLineThickness : s_Settings.MinorLineThickness);

			for (int i = -numCells; i <= numCells; i++) {
				// Every 10 lines is a major line (but not the center)
				bool isMajor = (i % 10 == 0) && (i != 0);
				if (isMajor != majorPass) continue;

				glm::vec4 color = isMajor ? majorColor : lineColor;
				float t = i * cellSize;
				glm::vec3 zStart(t, 0.0f, -gridExtent), zEnd(t, 0.0f, gridExtent);
				glm::vec3 xStart(-gridExtent, 0.0f, t), xEnd(gridExtent, 0.0f, t);
				Renderer2D::DrawLine(zStart, zEnd, color);   // parallel to Z
				Renderer2D::DrawLine(xStart, xEnd, color);   // parallel to X
			}
		}

		// Restore previous line width
		Renderer2D::SetLineWidth(previousLineWidth);
	}

}
