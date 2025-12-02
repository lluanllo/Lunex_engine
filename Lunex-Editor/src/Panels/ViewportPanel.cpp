#include "ViewportPanel.h"
#include "ContentBrowserPanel.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Core/Application.h"
#include "Core/Input.h"
#include "Scene/Components.h"
#include "Math/Math.h"
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	void ViewportPanel::OnImGuiRender(Ref<Framebuffer> framebuffer, SceneHierarchyPanel& hierarchyPanel,
									 const EditorCamera& editorCamera, Entity selectedEntity, int gizmoType,
									 ToolbarPanel& toolbarPanel, GizmoSettingsPanel& gizmoSettingsPanel,
									 SceneState sceneState, bool toolbarEnabled) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		// Renderizar la imagen del framebuffer
		uint64_t textureID = framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		// Drag & drop de escenas desde Content Browser
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				
				// Validar que sea un archivo .lunex (escena)
				std::string ext = data->Extension;
				if (ext == ".lunex") {
					if (m_OnSceneDropCallback) {
						m_OnSceneDropCallback(data->FilePath);
					}
				}
				else {
					LNX_LOG_WARN("Dropped file is not a scene file (.lunex)");
				}
			}
			ImGui::EndDragDropTarget();
		}

		// ========================================
		// GIZMOS WITH PIVOT POINT SUPPORT
		// ========================================
		const auto& selectedEntities = hierarchyPanel.GetSelectedEntities();
		
		// Only show gizmo if we have selection and gizmo type is valid
		if (!selectedEntities.empty() && gizmoType != -1) {
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, 
							  m_ViewportBounds[1].x - m_ViewportBounds[0].x, 
							  m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			// Editor camera
			const glm::mat4& cameraProjection = editorCamera.GetProjection();
			glm::mat4 cameraView = editorCamera.GetViewMatrix();

			// ========================================
			// CALCULATE PIVOT POINT POSITION
			// ========================================
			glm::vec3 pivotPosition(0.0f);
			PivotPoint pivotMode = gizmoSettingsPanel.GetPivotPoint();
			
			switch (pivotMode) {
				case PivotPoint::MedianPoint:
					pivotPosition = hierarchyPanel.CalculateMedianPoint();
					break;
				
				case PivotPoint::ActiveElement:
					pivotPosition = hierarchyPanel.CalculateActiveElementPosition();
					break;
				
				case PivotPoint::BoundingBox:
					pivotPosition = hierarchyPanel.CalculateBoundingBoxCenter();
					break;
				
				case PivotPoint::IndividualOrigins:
					// Individual origins: render gizmo for each selected entity
					// TODO: Implement individual gizmos (future feature)
					pivotPosition = hierarchyPanel.CalculateMedianPoint(); // Fallback for now
					break;
				
				case PivotPoint::Cursor3D:
					// 3D Cursor: use cursor position (future feature)
					pivotPosition = glm::vec3(0.0f); // Fallback for now
					break;
			}
			
			// Create transform matrix at pivot position
			glm::mat4 pivotTransform = glm::translate(glm::mat4(1.0f), pivotPosition);
			
			// ========================================
			// SNAPPING
			// ========================================
			bool snap = Input::IsKeyPressed(Key::LeftControl);
			float snapValue = 0.5f;
			if (gizmoType == ImGuizmo::OPERATION::ROTATE)
				snapValue = 45.0f;

			float snapValues[3] = { snapValue, snapValue, snapValue };

			// ========================================
			// TRANSFORM ORIENTATION
			// ========================================
			ImGuizmo::MODE gizmoMode = ImGuizmo::LOCAL;
			TransformOrientation orientation = gizmoSettingsPanel.GetOrientation();
			
			if (orientation == TransformOrientation::Global) {
				gizmoMode = ImGuizmo::WORLD;
			}
			else if (orientation == TransformOrientation::Local) {
				// Use local space of active element
				Entity activeEntity = hierarchyPanel.GetActiveEntity();
				if (activeEntity && activeEntity.HasComponent<TransformComponent>()) {
					// TODO: Apply local rotation to gizmo (advanced feature)
					gizmoMode = ImGuizmo::LOCAL;
				}
			}

			// ========================================
			// RENDER GIZMO
			// ========================================
			ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)gizmoType, gizmoMode, glm::value_ptr(pivotTransform),
				nullptr, snap ? snapValues : nullptr);

			// ========================================
			// APPLY TRANSFORMATION TO ALL SELECTED ENTITIES
			// ========================================
			bool isUsing = ImGuizmo::IsUsing();
			
			// Detectar inicio de transformación
			if (isUsing && !m_GizmoWasUsing) {
				// Guardar estado inicial
				Math::DecomposeTransform(pivotTransform, m_PreviousPivotPosition, m_PreviousPivotRotation, m_PreviousPivotScale);
			}
			
			if (isUsing) {
				glm::vec3 newPivotPosition, newRotation, newScale;
				Math::DecomposeTransform(pivotTransform, newPivotPosition, newRotation, newScale);
				
				// ? Calculate REAL deltas from previous frame
				glm::vec3 deltaPosition = newPivotPosition - m_PreviousPivotPosition;
				glm::vec3 deltaRotation = newRotation - m_PreviousPivotRotation;
				glm::vec3 deltaScale = newScale / m_PreviousPivotScale;
				
				// Apply transformation to all selected entities
				for (auto entity : selectedEntities) {
					if (entity.HasComponent<TransformComponent>()) {
						auto& tc = entity.GetComponent<TransformComponent>();
						
						if (pivotMode == PivotPoint::IndividualOrigins) {
							// Individual origins: each entity transforms around itself
							if (gizmoType == ImGuizmo::OPERATION::TRANSLATE) {
								tc.Translation += deltaPosition;
							}
							else if (gizmoType == ImGuizmo::OPERATION::ROTATE) {
								tc.Rotation += deltaRotation;
							}
							else if (gizmoType == ImGuizmo::OPERATION::SCALE) {
								tc.Scale *= deltaScale;
							}
						} else {
							// ? Apply translation delta
							if (gizmoType == ImGuizmo::OPERATION::TRANSLATE) {
								tc.Translation += deltaPosition;
							}
							// ? Apply rotation delta (around pivot) - CIRCULAR MOTION
							else if (gizmoType == ImGuizmo::OPERATION::ROTATE) {
								// Get offset from pivot BEFORE rotation
								glm::vec3 offsetFromPivot = tc.Translation - pivotPosition;
								
								// Create rotation matrices for each axis
								glm::mat4 rotationMatrix = glm::mat4(1.0f);
								
								// Apply delta rotations (in radians)
								if (std::abs(deltaRotation.x) > 0.0001f) {
									rotationMatrix = glm::rotate(rotationMatrix, deltaRotation.x, glm::vec3(1, 0, 0));
								}
								if (std::abs(deltaRotation.y) > 0.0001f) {
									rotationMatrix = glm::rotate(rotationMatrix, deltaRotation.y, glm::vec3(0, 1, 0));
								}
								if (std::abs(deltaRotation.z) > 0.0001f) {
									rotationMatrix = glm::rotate(rotationMatrix, deltaRotation.z, glm::vec3(0, 0, 1));
								}
								
								// Rotate offset around pivot
								glm::vec4 rotatedOffset = rotationMatrix * glm::vec4(offsetFromPivot, 1.0f);
								
								// Update entity position (circular motion around pivot)
								tc.Translation = pivotPosition + glm::vec3(rotatedOffset);
								
								// Also rotate the entity itself
								tc.Rotation += deltaRotation;
							}
							// ? Apply scale delta (from pivot)
							else if (gizmoType == ImGuizmo::OPERATION::SCALE) {
								// Scale entity position from pivot
								glm::vec3 offsetFromPivot = tc.Translation - pivotPosition;
								tc.Translation = pivotPosition + (offsetFromPivot * deltaScale);
								tc.Scale *= deltaScale;
							}
						}
					}
				}
				
				// Update previous state for next frame
				m_PreviousPivotPosition = newPivotPosition;
				m_PreviousPivotRotation = newRotation;
				m_PreviousPivotScale = newScale;
			}
			
			m_GizmoWasUsing = isUsing;
		}

		ImGui::End();
		ImGui::PopStyleVar();
		
		// Renderizar toolbar flotante DESPUÉS del viewport y gizmos
		// El toolbar debe renderizarse al final para estar encima de todo
		toolbarPanel.OnImGuiRender(sceneState, toolbarEnabled, m_ViewportBounds[0], m_ViewportSize);
	}
}
