/**
 * @file OutlinePreferencesPanel.h
 * @brief Outline & Collider Preferences Panel
 *
 * Opened from Preferences menu. Controls:
 * - Selection outline color, thickness, hardness, inside alpha
 * - 2D / 3D collider wireframe color and line width
 */

#pragma once

#include "../UI/LunexUI.h"
#include <glm/glm.hpp>

namespace Lunex {

	class OutlinePreferencesPanel {
	public:
		OutlinePreferencesPanel() = default;
		~OutlinePreferencesPanel() = default;

		void OnImGuiRender();

		void Open()   { m_Open = true; }
		void Close()  { m_Open = false; }
		void Toggle() { m_Open = !m_Open; }
		bool IsOpen() const { return m_Open; }

		// Selection Outline
		glm::vec4 GetOutlineColor() const { return m_OutlineColor; }
		int   GetOutlineKernelSize() const { return m_OutlineKernelSize; }
		float GetOutlineHardness() const { return m_OutlineHardness; }
		float GetOutlineInsideAlpha() const { return m_OutlineInsideAlpha; }
		bool  GetShowBehindObjects() const { return m_ShowBehindObjects; }

		// Collider Appearance
		glm::vec4 GetCollider2DColor() const { return m_Collider2DColor; }
		glm::vec4 GetCollider3DColor() const { return m_Collider3DColor; }
		float GetColliderLineWidth() const { return m_ColliderLineWidth; }

	private:
		void DrawSelectionOutlineSection();
		void DrawColliderAppearanceSection();

	private:
		bool m_Open = false;

		// Selection Outline
		glm::vec4 m_OutlineColor      = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
		int   m_OutlineKernelSize      = 3;
		float m_OutlineHardness        = 0.75f;
		float m_OutlineInsideAlpha     = 0.0f;
		bool  m_ShowBehindObjects      = true;

		// Collider Appearance
		glm::vec4 m_Collider2DColor   = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
		glm::vec4 m_Collider3DColor   = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
		float m_ColliderLineWidth      = 4.0f;
	};

} // namespace Lunex
