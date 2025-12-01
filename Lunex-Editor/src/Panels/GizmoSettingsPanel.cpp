#include "GizmoSettingsPanel.h"
#include "imgui.h"
#include "Log/Log.h"

namespace Lunex {
	void GizmoSettingsPanel::OnImGuiRender(const glm::vec2& viewportBounds, 
											const glm::vec2& viewportSize,
											bool toolbarEnabled) {
		// No crear ventana si el viewport es muy pequeño
		if (viewportSize.x < 100 || viewportSize.y < 100)
			return;

		// Configuración del panel
		float buttonSize = 28.0f;
		float spacing = 6.0f;
		float padding = 12.0f;
		
		// Número de botones en cada sección
		int pivotButtons = 4;  // Median, Active, Individual, BoundingBox
		int orientationButtons = 2;  // Global, Local
		int totalSections = 2;
		
		float sectionHeight = buttonSize + (padding * 0.5f);
		float separatorHeight = 4.0f;
		float totalHeight = (sectionHeight * totalSections) + separatorHeight + (padding * 2);
		float totalWidth = buttonSize + (padding * 2);
		
		// Posición: lado izquierdo del viewport, debajo de la toolbar
		float panelX = viewportBounds.x + 10.0f;
		float panelY = viewportBounds.y + 60.0f;  // Debajo de la toolbar principal
		
		ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
		
		// Estilo del contenedor (semi-transparente con bordes sutiles)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, spacing));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		
		// Contenedor completamente transparente
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | 
									   ImGuiWindowFlags_NoMove | 
									   ImGuiWindowFlags_NoScrollbar | 
									   ImGuiWindowFlags_NoScrollWithMouse |
									   ImGuiWindowFlags_NoSavedSettings |
									   ImGuiWindowFlags_NoFocusOnAppearing |
									   ImGuiWindowFlags_NoDocking;
		
		ImGui::Begin("##GizmoSettings", nullptr, windowFlags);
		
		ImVec4 tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		if (!toolbarEnabled)
			tintColor.w = 0.4f;
		
		// ========================================
		// SECTION 1: PIVOT POINT
		// ========================================
		RenderPivotPointButtons(buttonSize);
		
		// Separador visual
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.38f, 0.8f));
		ImGui::Separator();
		ImGui::PopStyleColor();
		ImGui::Spacing();
		
		// ========================================
		// SECTION 2: ORIENTATION
		// ========================================
		RenderOrientationButtons(buttonSize);
		
		ImGui::End();
		
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}

	void GizmoSettingsPanel::RenderPivotPointButtons(float buttonSize) {
		// ? Estilo idéntico a ToolbarPanel (translúcido con bordes redondeados)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, 0.9f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		
		// Color para botón activo (azul brillante)
		ImVec4 activeColor = ImVec4(0.26f, 0.59f, 0.98f, 0.9f);
		ImVec4 activeHoverColor = ImVec4(0.36f, 0.69f, 1.0f, 1.0f);
		
		ImVec4 tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		
		// ========== MEDIAN POINT BUTTON ==========
		{
			ImGui::PushID("PivotMedianPoint");
			
			bool isActive = (m_PivotPoint == PivotPoint::MedianPoint);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconMedianPoint) {
				clicked = ImGui::ImageButton("##MedianBtn",
					(ImTextureID)(intptr_t)m_IconMedianPoint->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("?", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_PivotPoint = PivotPoint::MedianPoint;
				LNX_LOG_INFO("Pivot Point: Median Point");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Median Point");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Transform around center of selection");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		// ========== ACTIVE ELEMENT BUTTON ==========
		{
			ImGui::PushID("PivotActiveElement");
			
			bool isActive = (m_PivotPoint == PivotPoint::ActiveElement);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconActiveElement) {
				clicked = ImGui::ImageButton("##ActiveBtn",
					(ImTextureID)(intptr_t)m_IconActiveElement->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("?", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_PivotPoint = PivotPoint::ActiveElement;
				LNX_LOG_INFO("Pivot Point: Active Element");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Active Element");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Transform around last selected object");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		// ========== INDIVIDUAL ORIGINS BUTTON ==========
		{
			ImGui::PushID("PivotIndividualOrigins");
			
			bool isActive = (m_PivotPoint == PivotPoint::IndividualOrigins);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconIndividualOrigins) {
				clicked = ImGui::ImageButton("##IndividualBtn",
					(ImTextureID)(intptr_t)m_IconIndividualOrigins->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("?", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_PivotPoint = PivotPoint::IndividualOrigins;
				LNX_LOG_INFO("Pivot Point: Individual Origins");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Individual Origins");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Each object transforms around itself");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		// ========== BOUNDING BOX CENTER BUTTON ==========
		{
			ImGui::PushID("PivotBoundingBox");
			
			bool isActive = (m_PivotPoint == PivotPoint::BoundingBox);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconBoundingBox) {
				clicked = ImGui::ImageButton("##BoundingBtn",
					(ImTextureID)(intptr_t)m_IconBoundingBox->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("?", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_PivotPoint = PivotPoint::BoundingBox;
				LNX_LOG_INFO("Pivot Point: Bounding Box Center");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Bounding Box Center");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Transform around bounding box center");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
	}

	void GizmoSettingsPanel::RenderOrientationButtons(float buttonSize) {
		// ? Estilo idéntico a ToolbarPanel
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, 0.9f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		
		// Color para botón activo
		ImVec4 activeColor = ImVec4(0.26f, 0.59f, 0.98f, 0.9f);
		ImVec4 activeHoverColor = ImVec4(0.36f, 0.69f, 1.0f, 1.0f);
		
		ImVec4 tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		
		// ========== GLOBAL ORIENTATION BUTTON ==========
		{
			ImGui::PushID("OrientGlobal");
			
			bool isActive = (m_Orientation == TransformOrientation::Global);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconGlobal) {
				clicked = ImGui::ImageButton("##GlobalBtn",
					(ImTextureID)(intptr_t)m_IconGlobal->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("??", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_Orientation = TransformOrientation::Global;
				LNX_LOG_INFO("Transform Orientation: Global");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Global Orientation");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Transform in world space");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		// ========== LOCAL ORIENTATION BUTTON ==========
		{
			ImGui::PushID("OrientLocal");
			
			bool isActive = (m_Orientation == TransformOrientation::Local);
			if (isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeHoverColor);
			}
			
			bool clicked = false;
			if (m_IconLocal) {
				clicked = ImGui::ImageButton("##LocalBtn",
					(ImTextureID)(intptr_t)m_IconLocal->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button("??", ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked) {
				m_Orientation = TransformOrientation::Local;
				LNX_LOG_INFO("Transform Orientation: Local");
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("Local Orientation");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("Transform in object's local space");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
			
			if (isActive) {
				ImGui::PopStyleColor(3);
			}
			
			ImGui::PopID();
		}
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
	}
}
