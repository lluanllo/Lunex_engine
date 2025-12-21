#include "ViewportPanel.h"
#include "ContentBrowserPanel.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Core/Application.h"
#include "Core/Input.h"
#include "Scene/Components.h"
#include "Math/Math.h"
#include "Assets/Mesh/MeshImporter.h"
#include "Asset/Prefab.h"
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	void ViewportPanel::OnImGuiRender(Ref<Framebuffer> framebuffer, Ref<Framebuffer> cameraPreviewFramebuffer,
		SceneHierarchyPanel& hierarchyPanel,
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

		// Drag & drop desde Content Browser
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;
				std::filesystem::path filePath = data->FilePath;

				// Scene files (.lunex)
				if (ext == ".lunex") {
					if (m_OnSceneDropCallback) {
						m_OnSceneDropCallback(filePath);
					}
				}
				// MeshAsset files (.lumesh) - create entity directly
				else if (ext == ".lumesh") {
					if (m_OnMeshAssetDropCallback) {
						m_OnMeshAssetDropCallback(filePath);
					}
				}
				// Prefab files (.luprefab) - instantiate prefab
				else if (ext == ".luprefab") {
					if (m_OnPrefabDropCallback) {
						m_OnPrefabDropCallback(filePath);
					}
				}
				// 3D model files - open import modal
				else if (MeshImporter::IsSupported(filePath)) {
					if (m_OnModelDropCallback) {
						m_OnModelDropCallback(filePath);
					}
				}
				else {
					LNX_LOG_WARN("Unsupported file type dropped on viewport: {0}", ext);
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

		// ========================================
		// CAMERA PREVIEW (Bottom-right corner)
		// ========================================
		if (cameraPreviewFramebuffer && selectedEntity && selectedEntity.HasComponent<CameraComponent>()) {
			RenderCameraPreview(cameraPreviewFramebuffer, selectedEntity);
		}

		ImGui::End();
		ImGui::PopStyleVar();

		// Renderizar toolbar flotante DESPUÉS del viewport y gizmos
		// El toolbar debe renderizarse al final para estar encima de todo
		toolbarPanel.OnImGuiRender(sceneState, toolbarEnabled, m_ViewportBounds[0], m_ViewportSize);
	}

	void ViewportPanel::RenderCameraPreview(Ref<Framebuffer> previewFramebuffer, Entity selectedEntity) {
		// Get the actual framebuffer size (matches camera aspect ratio)
		auto spec = previewFramebuffer->GetSpecification();
		float previewWidth = static_cast<float>(spec.Width);
		float previewHeight = static_cast<float>(spec.Height);
		float padding = 16.0f;
		
		// Calculate position (bottom-right corner of viewport)
		ImVec2 previewPos = ImVec2(
			m_ViewportBounds[1].x - previewWidth - padding,
			m_ViewportBounds[1].y - previewHeight - padding
		);
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Draw shadow (offset behind the image)
		float shadowOffset = 4.0f;
		float shadowBlur = 8.0f;
		ImVec2 shadowMin = ImVec2(previewPos.x + shadowOffset, previewPos.y + shadowOffset);
		ImVec2 shadowMax = ImVec2(previewPos.x + previewWidth + shadowOffset + shadowBlur, 
								  previewPos.y + previewHeight + shadowOffset + shadowBlur);
		drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 100), 6.0f);
		
		// Draw camera preview image
		uint64_t textureID = previewFramebuffer->GetColorAttachmentRendererID();
		ImVec2 imageMin = previewPos;
		ImVec2 imageMax = ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight);
		
		drawList->AddImage(
			reinterpret_cast<void*>(textureID),
			imageMin,
			imageMax,
			ImVec2(0, 1), ImVec2(1, 0)  // Flip Y
		);
		
		// Subtle border
		drawList->AddRect(imageMin, imageMax, IM_COL32(60, 60, 70, 150), 0.0f, 0, 1.0f);
	}
}