/**
 * @file GizmoSettingsPanel.cpp
 * @brief Gizmo Settings Panel - Pivot point and orientation controls
 * 
 * Features:
 * - Pivot Point selection (Median, Active, Individual, BoundingBox)
 * - Transform Orientation (Global, Local)
 * - Blender-style toolbar icons
 * - Floating panel on viewport left side
 */

#include "GizmoSettingsPanel.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	// ============================================================================
	// GIZMO PANEL CONSTANTS
	// ============================================================================
	
	namespace {
		constexpr float kButtonSize = 28.0f;
		constexpr float kSpacing = 6.0f;
		constexpr float kPadding = 12.0f;
		constexpr int kPivotButtonCount = 4;
		constexpr int kOrientationButtonCount = 2;
		constexpr float kSectionGap = 8.0f;
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void GizmoSettingsPanel::OnImGuiRender(const glm::vec2& viewportBounds,
										   const glm::vec2& viewportSize,
										   bool toolbarEnabled) {
		using namespace UI;
		
		// Skip if viewport too small
		if (viewportSize.x < 100 || viewportSize.y < 100) return;
		
		// Calculate dimensions
		float totalHeight = (kButtonSize * (kPivotButtonCount + kOrientationButtonCount)) +
							(kSpacing * (kPivotButtonCount + kOrientationButtonCount)) +
							kSectionGap + (kPadding * 2);
		float totalWidth = kButtonSize + (kPadding * 2);
		
		// Position: left side of viewport, below toolbar
		float panelX = viewportBounds.x + 10.0f;
		float panelY = viewportBounds.y + 60.0f;
		
		ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
		
		// Transparent container style
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPadding, kPadding));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, kSpacing));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		
		ImGuiWindowFlags windowFlags = 
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoDocking;
		
		ImGui::Begin("##GizmoSettings", nullptr, windowFlags);
		
		// Pivot Point Buttons
		RenderPivotPointButtons(kButtonSize, toolbarEnabled);
		
		// Section gap
		ImGui::Dummy(ImVec2(0, kSectionGap));
		
		// Orientation Buttons
		RenderOrientationButtons(kButtonSize, toolbarEnabled);
		
		ImGui::End();
		
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}

	// ============================================================================
	// PIVOT POINT BUTTONS
	// ============================================================================

	void GizmoSettingsPanel::RenderPivotPointButtons(float buttonSize, bool enabled) {
		using namespace UI;
		
		Color tint = enabled ? Colors::TextPrimary() : Colors::TextMuted();
		
		// Median Point
		{
			ToolbarButtonProps props;
			props.Id = "PivotMedianPoint";
			props.Icon = m_IconMedianPoint;
			props.FallbackText = "⊕";
			props.Size = buttonSize;
			props.IsSelected = (m_PivotPoint == PivotPoint::MedianPoint);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Median Point";
			props.TooltipDescription = "Transform around center of selection";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_PivotPoint = PivotPoint::MedianPoint;
				LNX_LOG_INFO("Pivot Point: Median Point");
			}
		}
		
		// Active Element
		{
			ToolbarButtonProps props;
			props.Id = "PivotActiveElement";
			props.Icon = m_IconActiveElement;
			props.FallbackText = "⊙";
			props.Size = buttonSize;
			props.IsSelected = (m_PivotPoint == PivotPoint::ActiveElement);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Active Element";
			props.TooltipDescription = "Transform around last selected object";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_PivotPoint = PivotPoint::ActiveElement;
				LNX_LOG_INFO("Pivot Point: Active Element");
			}
		}
		
		// Individual Origins
		{
			ToolbarButtonProps props;
			props.Id = "PivotIndividualOrigins";
			props.Icon = m_IconIndividualOrigins;
			props.FallbackText = "◉";
			props.Size = buttonSize;
			props.IsSelected = (m_PivotPoint == PivotPoint::IndividualOrigins);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Individual Origins";
			props.TooltipDescription = "Each object transforms around itself";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_PivotPoint = PivotPoint::IndividualOrigins;
				LNX_LOG_INFO("Pivot Point: Individual Origins");
			}
		}
		
		// Bounding Box Center
		{
			ToolbarButtonProps props;
			props.Id = "PivotBoundingBox";
			props.Icon = m_IconBoundingBox;
			props.FallbackText = "▢";
			props.Size = buttonSize;
			props.IsSelected = (m_PivotPoint == PivotPoint::BoundingBox);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Bounding Box Center";
			props.TooltipDescription = "Transform around bounding box center";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_PivotPoint = PivotPoint::BoundingBox;
				LNX_LOG_INFO("Pivot Point: Bounding Box Center");
			}
		}
	}

	// ============================================================================
	// ORIENTATION BUTTONS
	// ============================================================================

	void GizmoSettingsPanel::RenderOrientationButtons(float buttonSize, bool enabled) {
		using namespace UI;
		
		Color tint = enabled ? Colors::TextPrimary() : Colors::TextMuted();
		
		// Global Orientation
		{
			ToolbarButtonProps props;
			props.Id = "OrientGlobal";
			props.Icon = m_IconGlobal;
			props.FallbackText = "🌍";
			props.Size = buttonSize;
			props.IsSelected = (m_Orientation == TransformOrientation::Global);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Global Orientation";
			props.TooltipDescription = "Transform in world space";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_Orientation = TransformOrientation::Global;
				LNX_LOG_INFO("Transform Orientation: Global");
			}
		}
		
		// Local Orientation
		{
			ToolbarButtonProps props;
			props.Id = "OrientLocal";
			props.Icon = m_IconLocal;
			props.FallbackText = "📍";
			props.Size = buttonSize;
			props.IsSelected = (m_Orientation == TransformOrientation::Local);
			props.IsEnabled = enabled;
			props.TooltipTitle = "Local Orientation";
			props.TooltipDescription = "Transform in object's local space";
			props.Tint = tint;
			
			if (ToolbarButton(props)) {
				m_Orientation = TransformOrientation::Local;
				LNX_LOG_INFO("Transform Orientation: Local");
			}
		}
	}

} // namespace Lunex
