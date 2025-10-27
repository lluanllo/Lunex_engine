#include "stpch.h"
#include "ViewportPanel.h"

#include "Core/Input.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Math/Math.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;
	
	void ViewportPanel::OnImGuiRender() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
		
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
		
		uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		
		// Drag and drop para abrir escenas
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const wchar_t* path = (const wchar_t*)payload->Data;
				if (m_Callbacks.OnSceneDropped) {
					m_Callbacks.OnSceneDropped(std::filesystem::path(g_AssetPath) / path);
				}
			}
			ImGui::EndDragDropTarget();
		}
		
		// Gizmos
		if (m_Callbacks.GetSelectedEntity) {
			Entity selectedEntity = m_Callbacks.GetSelectedEntity();
			if (selectedEntity && m_GizmoType != -1) {
				RenderGizmos(selectedEntity);
			}
		}
		
		ImGui::End();
		ImGui::PopStyleVar();
	}
	
	void ViewportPanel::RenderGizmos(Entity selectedEntity) {
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		
		ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
			m_ViewportBounds[1].x - m_ViewportBounds[0].x,
			m_ViewportBounds[1].y - m_ViewportBounds[0].y);
		
		// Editor camera
		const EditorCamera& editorCamera = m_Callbacks.GetEditorCamera();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();
		glm::mat4 cameraView = editorCamera.GetViewMatrix();
		
		// Entity transform
		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.GetTransform();
		
		// Snapping
		bool snap = Input::IsKeyPressed(Key::LeftControl);
		float snapValue = 0.5f;
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f;
		
		float snapValues[3] = { snapValue, snapValue, snapValue };
		
		ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
			(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
			nullptr, snap ? snapValues : nullptr);
		
		if (ImGuizmo::IsUsing()) {
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);
			
			glm::vec3 deltaRotation = rotation - tc.Rotation;
			tc.Translation = translation;
			tc.Rotation += deltaRotation;
			tc.Scale = scale;
		}
	}
}