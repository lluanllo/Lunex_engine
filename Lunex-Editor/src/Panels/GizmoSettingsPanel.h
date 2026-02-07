/**
 * @file GizmoSettingsPanel.h
 * @brief Gizmo Settings Panel - Pivot point and orientation controls
 * 
 * Features:
 * - Pivot Point selection (Median, Active, Individual, BoundingBox)
 * - Transform Orientation (Global, Local)
 * - Blender-style toolbar icons
 * - Floating panel on viewport left side
 */

#pragma once

#include "Renderer/Texture.h"
#include "Scene/Scene.h"

#include <glm/glm.hpp>
#include <functional>

namespace Lunex {

	// ============================================================================
	// PIVOT POINT MODES (Blender-style)
	// ============================================================================
	
	enum class PivotPoint {
		MedianPoint,       // Center of all selected objects
		ActiveElement,     // Position of the last selected object
		IndividualOrigins, // Each object transforms around its own origin
		Cursor3D,          // Transform around 3D cursor (future feature)
		BoundingBox        // Center of bounding box
	};

	// ============================================================================
	// TRANSFORM ORIENTATION (Blender-style)
	// ============================================================================
	
	enum class TransformOrientation {
		Global,  // World space
		Local,   // Object's local space
		View     // Camera view space (future feature)
	};

	// ============================================================================
	// GIZMO SETTINGS PANEL
	// ============================================================================

	class GizmoSettingsPanel {
	public:
		GizmoSettingsPanel() = default;
		~GizmoSettingsPanel() = default;

		void OnImGuiRender(const glm::vec2& viewportBounds, 
						   const glm::vec2& viewportSize, 
						   bool toolbarEnabled);

		// Getters
		PivotPoint GetPivotPoint() const { return m_PivotPoint; }
		TransformOrientation GetOrientation() const { return m_Orientation; }
		bool IsProportionalEditingEnabled() const { return m_ProportionalEditing; }

		// Setters
		void SetPivotPoint(PivotPoint pivot) { m_PivotPoint = pivot; }
		void SetOrientation(TransformOrientation orientation) { m_Orientation = orientation; }

		// Icon setters
		void SetMedianPointIcon(Ref<Texture2D> icon) { m_IconMedianPoint = icon; }
		void SetActiveElementIcon(Ref<Texture2D> icon) { m_IconActiveElement = icon; }
		void SetIndividualOriginsIcon(Ref<Texture2D> icon) { m_IconIndividualOrigins = icon; }
		void SetBoundingBoxIcon(Ref<Texture2D> icon) { m_IconBoundingBox = icon; }
		void SetGlobalIcon(Ref<Texture2D> icon) { m_IconGlobal = icon; }
		void SetLocalIcon(Ref<Texture2D> icon) { m_IconLocal = icon; }

	private:
		// Render helpers
		void RenderPivotPointButtons(float buttonSize, bool enabled);
		void RenderOrientationButtons(float buttonSize, bool enabled);

	private:
		// Settings
		PivotPoint m_PivotPoint = PivotPoint::MedianPoint;
		TransformOrientation m_Orientation = TransformOrientation::Global;
		bool m_ProportionalEditing = false;

		// Pivot icons
		Ref<Texture2D> m_IconMedianPoint;
		Ref<Texture2D> m_IconActiveElement;
		Ref<Texture2D> m_IconIndividualOrigins;
		Ref<Texture2D> m_IconBoundingBox;

		// Orientation icons
		Ref<Texture2D> m_IconGlobal;
		Ref<Texture2D> m_IconLocal;
	};

} // namespace Lunex
