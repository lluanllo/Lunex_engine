/**
 * @file ViewportPanel.cpp
 * @brief Viewport Panel - Main 3D/2D scene viewport with gizmos
 * 
 * Features:
 * - Main scene framebuffer display
 * - ImGuizmo transform manipulation
 * - Drag & drop for scenes, models, prefabs
 * - Camera preview overlay for selected cameras
 * - Toolbar integration
 */

#include "ViewportPanel.h"
#include "ContentBrowserPanel.h"
#include "Core/Application.h"
#include "Core/Input.h"
#include "Scene/Components.h"
#include "Math/Math.h"
#include "Assets/Mesh/MeshImporter.h"
#include "Asset/Prefab.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	// ============================================================================
	// VIEWPORT STYLE CONSTANTS
	// ============================================================================
	
	namespace ViewportStyle {
		// Camera preview
		constexpr float PreviewPadding = 16.0f;
		constexpr float ShadowOffset = 4.0f;
		constexpr float ShadowBlur = 8.0f;
		
		inline UI::Color ShadowColor()    { return UI::Color(0.0f, 0.0f, 0.0f, 0.40f); }
		inline UI::Color BorderColor()    { return UI::Color(0.24f, 0.24f, 0.28f, 0.60f); }
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void ViewportPanel::OnImGuiRender(
		Ref<Framebuffer> framebuffer, 
		Ref<Framebuffer> cameraPreviewFramebuffer,
		SceneHierarchyPanel& hierarchyPanel,
		const EditorCamera& editorCamera, 
		Entity selectedEntity, 
		int gizmoType,
		ToolbarPanel& toolbarPanel, 
		SceneState sceneState, 
		bool toolbarEnabled) 
	{
		using namespace UI;

		// Viewport window with no padding for full framebuffer coverage
		ScopedStyle viewportPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		
		ImGui::Begin("Viewport");

		// Calculate viewport bounds
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		
		m_ViewportBounds[0] = { 
			viewportMinRegion.x + viewportOffset.x, 
			viewportMinRegion.y + viewportOffset.y 
		};
		m_ViewportBounds[1] = { 
			viewportMaxRegion.x + viewportOffset.x, 
			viewportMaxRegion.y + viewportOffset.y 
		};

		// Focus & hover state
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		// Viewport size
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		// Render framebuffer image
		RenderFramebufferImage(framebuffer);

		// Handle drag & drop
		HandleDragDrop();

		// Render gizmos
		if (selectedEntity && gizmoType != -1) {
			RenderGizmos(editorCamera, selectedEntity, gizmoType);
		}

		// Camera preview overlay
		if (cameraPreviewFramebuffer && selectedEntity && selectedEntity.HasComponent<CameraComponent>()) {
			RenderCameraPreview(cameraPreviewFramebuffer, selectedEntity);
		}

		ImGui::End();

		// Render floating toolbar AFTER viewport (on top)
		toolbarPanel.OnImGuiRender(sceneState, toolbarEnabled, m_ViewportBounds[0], m_ViewportSize);
	}

	// ============================================================================
	// FRAMEBUFFER IMAGE
	// ============================================================================

	void ViewportPanel::RenderFramebufferImage(Ref<Framebuffer> framebuffer) {
		uint64_t textureID = framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(
			reinterpret_cast<void*>(textureID), 
			ImVec2(m_ViewportSize.x, m_ViewportSize.y), 
			ImVec2(0, 1), 
			ImVec2(1, 0)
		);
	}

	// ============================================================================
	// DRAG & DROP HANDLING
	// ============================================================================

	void ViewportPanel::HandleDragDrop() {
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
	}

	// ============================================================================
	// GIZMOS
	// ============================================================================

	void ViewportPanel::RenderGizmos(const EditorCamera& editorCamera, Entity selectedEntity, int gizmoType) {
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect(
			m_ViewportBounds[0].x, 
			m_ViewportBounds[0].y,
			m_ViewportBounds[1].x - m_ViewportBounds[0].x,
			m_ViewportBounds[1].y - m_ViewportBounds[0].y
		);

		// Camera matrices
		const glm::mat4& cameraProjection = editorCamera.GetProjection();
		glm::mat4 cameraView = editorCamera.GetViewMatrix();

		// Entity transform
		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.GetTransform();

		// Snapping
		bool snap = Input::IsKeyPressed(Key::LeftControl);
		float snapValue = (gizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
		float snapValues[3] = { snapValue, snapValue, snapValue };

		// Manipulate
		ImGuizmo::Manipulate(
			glm::value_ptr(cameraView), 
			glm::value_ptr(cameraProjection),
			(ImGuizmo::OPERATION)gizmoType, 
			ImGuizmo::LOCAL, 
			glm::value_ptr(transform),
			nullptr, 
			snap ? snapValues : nullptr
		);

		// Apply changes if using gizmo
		if (ImGuizmo::IsUsing()) {
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			glm::vec3 deltaRotation = rotation - tc.Rotation;
			tc.Translation = translation;
			tc.Rotation += deltaRotation;
			tc.Scale = scale;
		}
	}

	// ============================================================================
	// CAMERA PREVIEW OVERLAY
	// ============================================================================

	void ViewportPanel::RenderCameraPreview(Ref<Framebuffer> previewFramebuffer, Entity selectedEntity) {
		using namespace UI;

		// Get framebuffer dimensions
		auto spec = previewFramebuffer->GetSpecification();
		float previewWidth = static_cast<float>(spec.Width);
		float previewHeight = static_cast<float>(spec.Height);
		
		// Position: bottom-right corner
		ImVec2 previewPos = ImVec2(
			m_ViewportBounds[1].x - previewWidth - ViewportStyle::PreviewPadding,
			m_ViewportBounds[1].y - previewHeight - ViewportStyle::PreviewPadding
		);
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Shadow
		ImVec2 shadowMin = ImVec2(
			previewPos.x + ViewportStyle::ShadowOffset, 
			previewPos.y + ViewportStyle::ShadowOffset
		);
		ImVec2 shadowMax = ImVec2(
			previewPos.x + previewWidth + ViewportStyle::ShadowOffset + ViewportStyle::ShadowBlur, 
			previewPos.y + previewHeight + ViewportStyle::ShadowOffset + ViewportStyle::ShadowBlur
		);
		drawList->AddRectFilled(shadowMin, shadowMax, ViewportStyle::ShadowColor().ToImU32(), 6.0f);
		
		// Preview image
		uint64_t textureID = previewFramebuffer->GetColorAttachmentRendererID();
		ImVec2 imageMin = previewPos;
		ImVec2 imageMax = ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight);
		
		drawList->AddImage(
			reinterpret_cast<void*>(textureID),
			imageMin,
			imageMax,
			ImVec2(0, 1), ImVec2(1, 0)  // Flip Y for OpenGL
		);
		
		// Border
		drawList->AddRect(imageMin, imageMax, ViewportStyle::BorderColor().ToImU32(), 0.0f, 0, 1.0f);
	}

} // namespace Lunex