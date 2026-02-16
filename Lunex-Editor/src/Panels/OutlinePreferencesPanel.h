/**
 * @file OutlinePreferencesPanel.h
 * @brief Outline & Collider Preferences Panel
 *
 * Opened from Preferences menu. Controls:
 * - Selection outline color, thickness, hardness, inside alpha
 * - 2D / 3D collider wireframe color and line width
 *
 * Auto-saves to the active project file on every change.
 */

#pragma once

#include "../UI/LunexUI.h"
#include "Project/Project.h"
#include <glm/glm.hpp>
#include <functional>

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

		// Gizmo Appearance (frustums, lights)
		float GetGizmoLineWidth() const { return m_GizmoLineWidth; }

		// Load/Save from project config
		void LoadFromConfig(const OutlinePreferencesConfig& config);
		void SaveToConfig(OutlinePreferencesConfig& config) const;

		// Callback invoked whenever any setting changes (for autosave)
		void SetOnChangedCallback(const std::function<void()>& callback) { m_OnChanged = callback; }

	private:
		void DrawSelectionOutlineSection();
		void DrawColliderAppearanceSection();
		void NotifyChanged();

	private:
		bool m_Open = false;

		// Selection Outline
		glm::vec4 m_OutlineColor      = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		int   m_OutlineKernelSize      = 3;
		float m_OutlineHardness        = 0.75f;
		float m_OutlineInsideAlpha     = 0.0f;
		bool  m_ShowBehindObjects      = true;

		// Collider Appearance
		glm::vec4 m_Collider2DColor   = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		glm::vec4 m_Collider3DColor   = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
		float m_ColliderLineWidth      = 4.0f;
		
		// Gizmo Appearance (frustums, lights)
		float m_GizmoLineWidth         = 1.5f;

		// Autosave callback
		std::function<void()> m_OnChanged;
	};

} // namespace Lunex
