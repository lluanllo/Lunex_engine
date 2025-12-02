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
		ToolbarPanel& toolbarPanel, SceneState sceneState, bool toolbarEnabled) {
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

		// Gizmos (se dibujan sobre la imagen del viewport)
		if (selectedEntity && gizmoType != -1) {
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
				m_ViewportBounds[1].x - m_ViewportBounds[0].x,
				m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			// Editor camera
			const glm::mat4& cameraProjection = editorCamera.GetProjection();
			glm::mat4 cameraView = editorCamera.GetViewMatrix();

			// Entity transform
			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			// Snapping
			bool snap = Input::IsKeyPressed(Key::LeftControl);
			float snapValue = 0.5f;
			if (gizmoType == ImGuizmo::OPERATION::ROTATE)
				snapValue = 45.0f;

			float snapValues[3] = { snapValue, snapValue, snapValue };

			ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)gizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
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

		ImGui::End();
		ImGui::PopStyleVar();

		// Renderizar toolbar flotante DESPUÉS del viewport y gizmos
		// El toolbar debe renderizarse al final para estar encima de todo
		toolbarPanel.OnImGuiRender(sceneState, toolbarEnabled, m_ViewportBounds[0], m_ViewportSize);
	}
}