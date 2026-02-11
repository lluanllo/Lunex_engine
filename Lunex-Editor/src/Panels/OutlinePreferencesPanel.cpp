/**
 * @file OutlinePreferencesPanel.cpp
 * @brief Outline & Collider Preferences Panel Implementation
 */

#include "stpch.h"
#include "OutlinePreferencesPanel.h"

#include "Renderer/Outline/OutlineRenderer.h"
#include "Log/Log.h"

#include <imgui.h>

namespace Lunex {

	void OutlinePreferencesPanel::OnImGuiRender() {
		if (!m_Open) return;

		using namespace UI;

		ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Outline Preferences", &m_Open)) {
			DrawSelectionOutlineSection();

			Separator();

			DrawColliderAppearanceSection();
		}
		ImGui::End();
	}

	// ========================================================================
	// SELECTION OUTLINE
	// ========================================================================

	void OutlinePreferencesPanel::DrawSelectionOutlineSection() {
		using namespace UI;

		if (BeginSection("Selection Outline", true)) {
			auto& outlineConfig = OutlineRenderer::Get().GetConfig();

			// Outline color
			Color outlineColor(m_OutlineColor);
			if (PropertyColor4("Color", outlineColor, "Color of the selection outline")) {
				m_OutlineColor = glm::vec4(outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a);
			}

			// Kernel size (outline thickness)
			int kernelSize = outlineConfig.KernelSize;
			BeginPropertyRow("Thickness", "Blur radius that controls outline width (1-10)");
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderInt("##OutlineKernelSize", &kernelSize, 1, 10)) {
				outlineConfig.KernelSize = kernelSize;
				m_OutlineKernelSize = kernelSize;
			}
			EndPropertyRow();

			// Hardness
			float hardness = outlineConfig.OutlineHardness;
			if (PropertySlider("Hardness", hardness, 0.0f, 1.0f, "%.2f", "0 = soft glow, 1 = hard edge")) {
				outlineConfig.OutlineHardness = hardness;
				m_OutlineHardness = hardness;
			}

			// Inside alpha
			float insideAlpha = outlineConfig.InsideAlpha;
			if (PropertySlider("Inside Alpha", insideAlpha, 0.0f, 1.0f, "%.2f", "Opacity inside the outlined shape")) {
				outlineConfig.InsideAlpha = insideAlpha;
				m_OutlineInsideAlpha = insideAlpha;
			}

			// Show behind objects
			bool showBehind = outlineConfig.ShowBehindObjects;
			if (PropertyCheckbox("Show Behind Objects", showBehind, "Outline visible through other geometry")) {
				outlineConfig.ShowBehindObjects = showBehind;
				m_ShowBehindObjects = showBehind;
			}

			EndSection();
		}
	}

	// ========================================================================
	// COLLIDER APPEARANCE
	// ========================================================================

	void OutlinePreferencesPanel::DrawColliderAppearanceSection() {
		using namespace UI;

		if (BeginSection("Collider Appearance", true)) {

			// 2D Collider color
			Color color2D(m_Collider2DColor);
			if (PropertyColor4("2D Color", color2D, "Color for Box2D / Circle2D collider outlines")) {
				m_Collider2DColor = glm::vec4(color2D.r, color2D.g, color2D.b, color2D.a);
			}

			// 3D Collider color
			Color color3D(m_Collider3DColor);
			if (PropertyColor4("3D Color", color3D, "Color for Bullet3D collider wireframes")) {
				m_Collider3DColor = glm::vec4(color3D.r, color3D.g, color3D.b, color3D.a);
			}

			// Collider line width
			if (PropertySlider("Line Width", m_ColliderLineWidth, 1.0f, 10.0f, "%.1f", "Width of collider wireframe lines")) {
				// Value updated by reference
			}

			EndSection();
		}
	}

} // namespace Lunex
