/**
 * @file ViewportPanel.cpp
 * @brief Viewport Panel - Main 3D/2D scene viewport with gizmos
 * 
 * Features:
 * - Main scene framebuffer display
 * - ImGuizmo transform manipulation
 * - Multi-selection gizmo with pivot point modes (Blender-style)
 * - Transform orientation: Global / Local
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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
		bool toolbarEnabled,
		GizmoSettingsPanel& gizmoSettings) 
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

		// Render gizmos (multi-selection aware)
		const auto& selectedEntities = hierarchyPanel.GetSelectedEntities();
		if (!selectedEntities.empty() && gizmoType != -1) {
			RenderGizmos(hierarchyPanel, editorCamera, gizmoType, gizmoSettings);
		}
		else {
			// Reset gizmo tracking state when nothing is selected
			m_GizmoWasUsing = false;
		}

		// Camera preview overlay
		if (cameraPreviewFramebuffer && selectedEntity && selectedEntity.HasComponent<CameraComponent>()) {
			RenderCameraPreview(cameraPreviewFramebuffer, selectedEntity);
		}

		// Path tracer HUD overlay (sample counter, progress, reset)
		RenderPathTracerOverlay();

		ImGui::End();

		// Render floating toolbar AFTER viewport (on top)
		toolbarPanel.OnImGuiRender(sceneState, toolbarEnabled, m_ViewportBounds[0], m_ViewportSize);
	}

	// ============================================================================
	// FRAMEBUFFER IMAGE
	// ============================================================================

	void ViewportPanel::RenderFramebufferImage(Ref<Framebuffer> framebuffer) {
		uint64_t textureID = 0;
		if (m_OverrideTextureID != 0) {
			textureID = static_cast<uint64_t>(m_OverrideTextureID);
		} else {
			textureID = framebuffer->GetColorAttachmentRendererID();
		}
		if (textureID == 0) return;
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
	// PATH TRACER OVERLAY — sample counter, progress bar, reset button
	// ============================================================================

	void ViewportPanel::RenderPathTracerOverlay() {
		if (!m_ActiveBackend) return;
		if (m_ActiveBackend->GetType() != RenderBackendType::PathTracer) return;
		if (!m_ActiveBackend->IsProgressiveRender()) return;

		auto stats = m_ActiveBackend->GetStats();
		uint32_t samples = stats.AccumulatedSamples;
		uint32_t maxSamples = m_ActiveBackend->GetSettings().MaxAccumulatedSamples;

		// Position: top-left of the viewport content area with a small margin
		constexpr float kPadding = 10.0f;
		constexpr float kOverlayWidth = 220.0f;

		ImVec2 overlayPos(m_ViewportBounds[0].x + kPadding,
		                  m_ViewportBounds[0].y + kPadding + 40.0f); // below toolbar

		ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.70f);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
		                       | ImGuiWindowFlags_AlwaysAutoResize
		                       | ImGuiWindowFlags_NoSavedSettings
		                       | ImGuiWindowFlags_NoFocusOnAppearing
		                       | ImGuiWindowFlags_NoNav
		                       | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

		if (ImGui::Begin("##PathTracerOverlay", nullptr, flags)) {
			ImGui::TextUnformatted("Path Tracer");
			ImGui::Separator();

			// Sample counter
			if (maxSamples > 0)
				ImGui::Text("Samples: %u / %u", samples, maxSamples);
			else
				ImGui::Text("Samples: %u", samples);

			// Progress bar (only when a cap is set)
			if (maxSamples > 0) {
				float progress = static_cast<float>(samples) / static_cast<float>(maxSamples);
				ImGui::ProgressBar(progress, ImVec2(kOverlayWidth - 16.0f, 0.0f));
			}

			// Tri / BVH info
			ImGui::Text("Tris: %u  BVH: %u", stats.TotalTriangles, stats.BVHNodeCount);

			// Reset button
			if (ImGui::SmallButton("Reset")) {
				m_ActiveBackend->ResetAccumulation();
			}
		}
		ImGui::End();

		ImGui::PopStyleVar(2);
	}

	// ============================================================================
	// GIZMOS - Multi-selection with Pivot Point & Orientation support
	// ============================================================================

	void ViewportPanel::RenderGizmos(
		SceneHierarchyPanel& hierarchyPanel,
		const EditorCamera& editorCamera,
		int gizmoType,
		GizmoSettingsPanel& gizmoSettings)
	{
		const auto& selectedEntities = hierarchyPanel.GetSelectedEntities();
		if (selectedEntities.empty()) return;

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

		// Snapping
		bool snap = Input::IsKeyPressed(Key::LeftControl);
		float snapValue = (gizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
		float snapValues[3] = { snapValue, snapValue, snapValue };

		PivotPoint pivotMode = gizmoSettings.GetPivotPoint();
		TransformOrientation orientMode = gizmoSettings.GetOrientation();

		// ========================================================================
		// INDIVIDUAL ORIGINS MODE: Each object gets its own gizmo
		// ========================================================================
		if (pivotMode == PivotPoint::IndividualOrigins && selectedEntities.size() > 1) {
			Entity activeEntity = hierarchyPanel.GetActiveEntity();
			if (!activeEntity || !activeEntity.HasComponent<TransformComponent>()) {
				activeEntity = *selectedEntities.begin();
			}

			for (auto entity : selectedEntities) {
				if (!entity.HasComponent<TransformComponent>()) continue;

				auto& tc = entity.GetComponent<TransformComponent>();
				glm::mat4 transform = tc.GetTransform();

				ImGuizmo::MODE mode = (orientMode == TransformOrientation::Local) 
					? ImGuizmo::LOCAL : ImGuizmo::WORLD;

				if (entity == activeEntity) {
					ImGuizmo::Manipulate(
						glm::value_ptr(cameraView),
						glm::value_ptr(cameraProjection),
						(ImGuizmo::OPERATION)gizmoType,
						mode,
						glm::value_ptr(transform),
						nullptr,
						snap ? snapValues : nullptr
					);

					if (ImGuizmo::IsUsing()) {
						glm::vec3 newTranslation, newRotation, newScale;
						Math::DecomposeTransform(transform, newTranslation, newRotation, newScale);

						glm::vec3 deltaTranslation = newTranslation - tc.Translation;
						glm::vec3 deltaRotation = newRotation - tc.Rotation;
						glm::vec3 deltaScale = newScale - tc.Scale;

						tc.Translation = newTranslation;
						tc.Rotation += deltaRotation;
						tc.Scale = newScale;

						for (auto otherEntity : selectedEntities) {
							if (otherEntity == entity) continue;
							if (!otherEntity.HasComponent<TransformComponent>()) continue;

							auto& otherTc = otherEntity.GetComponent<TransformComponent>();

							if (gizmoType == ImGuizmo::OPERATION::TRANSLATE) {
								otherTc.Translation += deltaTranslation;
							}
							else if (gizmoType == ImGuizmo::OPERATION::ROTATE) {
								otherTc.Rotation += deltaRotation;
							}
							else if (gizmoType == ImGuizmo::OPERATION::SCALE) {
								otherTc.Scale += deltaScale;
							}
						}
					}
				}
			}
			return;
		}

		// ========================================================================
		// For single selection, just use the entity's own transform
		// ========================================================================
		if (selectedEntities.size() == 1) {
			Entity entity = *selectedEntities.begin();
			if (!entity.HasComponent<TransformComponent>()) return;

			auto& tc = entity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			ImGuizmo::MODE mode = (orientMode == TransformOrientation::Local) 
				? ImGuizmo::LOCAL : ImGuizmo::WORLD;

			ImGuizmo::Manipulate(
				glm::value_ptr(cameraView),
				glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)gizmoType,
				mode,
				glm::value_ptr(transform),
				nullptr,
				snap ? snapValues : nullptr
			);

			if (ImGuizmo::IsUsing()) {
				glm::vec3 translation, rotation, scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);

				glm::vec3 deltaRotation = rotation - tc.Rotation;
				tc.Translation = translation;
				tc.Rotation += deltaRotation;
				tc.Scale = scale;
			}
			return;
		}

		// ========================================================================
		// MULTI-SELECTION: Shared pivot gizmo
		// ========================================================================

		// Get orientation from active entity for Local mode
		Entity activeEntity = hierarchyPanel.GetActiveEntity();

		// Build the pivot transform that ImGuizmo will display and manipulate.
		// CRITICAL: When already dragging, we must give ImGuizmo the SAME base
		// transform that it returned last frame (m_GizmoPreviousTransform),
		// otherwise the gizmo "resets" every frame causing trembling.
		glm::mat4 pivotTransform;
		if (m_GizmoWasUsing) {
			// While dragging: feed back the previous output so ImGuizmo 
			// sees continuity
			pivotTransform = m_GizmoPreviousTransform;
		}
		else {
			// Not dragging: compute fresh from live entity positions
			glm::vec3 livePivotPosition;
			switch (pivotMode) {
				case PivotPoint::ActiveElement:
					livePivotPosition = hierarchyPanel.CalculateActiveElementPosition();
					break;
				case PivotPoint::BoundingBox:
					livePivotPosition = hierarchyPanel.CalculateBoundingBoxCenter();
					break;
				case PivotPoint::MedianPoint:
				default:
					livePivotPosition = hierarchyPanel.CalculateMedianPoint();
					break;
			}

			if (orientMode == TransformOrientation::Local && activeEntity && activeEntity.HasComponent<TransformComponent>()) {
				auto& activeTc = activeEntity.GetComponent<TransformComponent>();
				glm::mat4 rotation = glm::toMat4(glm::quat(activeTc.Rotation));
				pivotTransform = glm::translate(glm::mat4(1.0f), livePivotPosition) * rotation;
			}
			else {
				pivotTransform = glm::translate(glm::mat4(1.0f), livePivotPosition);
			}
		}

		ImGuizmo::MODE imguizmoMode = (orientMode == TransformOrientation::Local) 
			? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		glm::mat4 manipulatedTransform = pivotTransform;

		ImGuizmo::Manipulate(
			glm::value_ptr(cameraView),
			glm::value_ptr(cameraProjection),
			(ImGuizmo::OPERATION)gizmoType,
			imguizmoMode,
			glm::value_ptr(manipulatedTransform),
			nullptr,
			snap ? snapValues : nullptr
		);

		bool isUsing = ImGuizmo::IsUsing();

		if (isUsing) {
			if (!m_GizmoWasUsing) {
				// Gizmo just started being used - cache everything

				// Compute the live pivot position at drag start
				glm::vec3 livePivotPosition;
				switch (pivotMode) {
					case PivotPoint::ActiveElement:
						livePivotPosition = hierarchyPanel.CalculateActiveElementPosition();
						break;
					case PivotPoint::BoundingBox:
						livePivotPosition = hierarchyPanel.CalculateBoundingBoxCenter();
						break;
					case PivotPoint::MedianPoint:
					default:
						livePivotPosition = hierarchyPanel.CalculateMedianPoint();
						break;
				}
				m_GizmoCachedPivot = livePivotPosition;

				// Cache initial per-entity transforms
				m_GizmoInitialEntityTranslations.clear();
				m_GizmoInitialEntityRotations.clear();
				for (auto entity : selectedEntities) {
					if (!entity.HasComponent<TransformComponent>()) continue;
					auto& tc = entity.GetComponent<TransformComponent>();
					entt::entity handle = (entt::entity)entity;
					m_GizmoInitialEntityTranslations[handle] = tc.Translation;
					m_GizmoInitialEntityRotations[handle] = tc.Rotation;
				}

				// The pivotTransform was already set correctly above (not-dragging branch)
				m_GizmoInitialPivotTransform = pivotTransform;
				m_GizmoPreviousTransform = pivotTransform;
				m_GizmoWasUsing = true;
			}

			if (gizmoType == ImGuizmo::OPERATION::TRANSLATE) {
				glm::vec3 prevTranslation, prevRotation, prevScale;
				Math::DecomposeTransform(m_GizmoPreviousTransform, prevTranslation, prevRotation, prevScale);

				glm::vec3 newTranslation, newRotation, newScale;
				Math::DecomposeTransform(manipulatedTransform, newTranslation, newRotation, newScale);

				glm::vec3 deltaTranslation = newTranslation - prevTranslation;

				for (auto entity : selectedEntities) {
					if (!entity.HasComponent<TransformComponent>()) continue;
					auto& tc = entity.GetComponent<TransformComponent>();
					tc.Translation += deltaTranslation;
				}

				// Update cached pivot to follow the translation
				m_GizmoCachedPivot += deltaTranslation;
				
				// Also update initial translations so rotation doesn't drift after translate
				for (auto& [handle, pos] : m_GizmoInitialEntityTranslations) {
					pos += deltaTranslation;
				}
			}
			else if (gizmoType == ImGuizmo::OPERATION::ROTATE) {
				// Compute the TOTAL rotation delta from drag start to current frame.
				// This avoids accumulating per-frame Euler angle errors.
				glm::mat3 initialRot3 = glm::mat3(m_GizmoInitialPivotTransform);
				glm::mat3 currentRot3 = glm::mat3(manipulatedTransform);

				// Normalize columns to remove any scale
				for (int i = 0; i < 3; i++) {
					initialRot3[i] = glm::normalize(initialRot3[i]);
					currentRot3[i] = glm::normalize(currentRot3[i]);
				}

				glm::quat initialQuat = glm::quat_cast(initialRot3);
				glm::quat currentQuat = glm::quat_cast(currentRot3);
				glm::quat totalDeltaQuat = glm::normalize(currentQuat * glm::inverse(initialQuat));

				for (auto entity : selectedEntities) {
					if (!entity.HasComponent<TransformComponent>()) continue;
					auto& tc = entity.GetComponent<TransformComponent>();
					entt::entity handle = (entt::entity)entity;

					// Get initial state from cache
					glm::vec3 initialPos = m_GizmoInitialEntityTranslations[handle];
					glm::vec3 initialRot = m_GizmoInitialEntityRotations[handle];

					// Rotate position around the cached pivot from initial position
					glm::vec3 relativePos = initialPos - m_GizmoCachedPivot;
					glm::vec3 rotatedPos = totalDeltaQuat * relativePos;
					tc.Translation = m_GizmoCachedPivot + rotatedPos;

					// Apply total rotation delta to the initial entity rotation
					glm::quat initialEntityQuat = glm::quat(initialRot);
					glm::quat newEntityQuat = totalDeltaQuat * initialEntityQuat;
					
					// Convert back to euler angles - safe because we compute from
					// initial state each frame rather than accumulating
					glm::vec3 newEuler = glm::eulerAngles(newEntityQuat);
					
					// Keep the euler angles continuous with the initial rotation
					for (int i = 0; i < 3; i++) {
						float diff = newEuler[i] - initialRot[i];
						if (diff > glm::pi<float>()) newEuler[i] -= glm::two_pi<float>();
						else if (diff < -glm::pi<float>()) newEuler[i] += glm::two_pi<float>();
					}
					
					tc.Rotation = newEuler;
				}
			}
			else if (gizmoType == ImGuizmo::OPERATION::SCALE) {
				glm::vec3 prevTranslation, prevRotation, prevScale;
				Math::DecomposeTransform(m_GizmoPreviousTransform, prevTranslation, prevRotation, prevScale);

				glm::vec3 newTranslation, newRotation, newScale;
				Math::DecomposeTransform(manipulatedTransform, newTranslation, newRotation, newScale);

				glm::vec3 deltaScale = newScale / prevScale;

				for (int i = 0; i < 3; i++) {
					if (std::abs(prevScale[i]) < 0.0001f) deltaScale[i] = 1.0f;
					deltaScale[i] = glm::clamp(deltaScale[i], 0.01f, 100.0f);
				}

				for (auto entity : selectedEntities) {
					if (!entity.HasComponent<TransformComponent>()) continue;
					auto& tc = entity.GetComponent<TransformComponent>();

					glm::vec3 relativePos = tc.Translation - m_GizmoCachedPivot;
					tc.Translation = m_GizmoCachedPivot + relativePos * deltaScale;

					tc.Scale *= deltaScale;
				}
			}

			// Update previous transform for next frame
			m_GizmoPreviousTransform = manipulatedTransform;
		}
		else {
			if (m_GizmoWasUsing) {
				// Clean up cached state
				m_GizmoInitialEntityTranslations.clear();
				m_GizmoInitialEntityRotations.clear();
			}
			m_GizmoWasUsing = false;
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
		
		if (textureID != 0) {
			drawList->AddImage(
				reinterpret_cast<void*>(textureID),
				imageMin,
				imageMax,
				ImVec2(0, 1), ImVec2(1, 0)  // Flip Y for OpenGL
			);
		}
		
		// Border
		drawList->AddRect(imageMin, imageMax, ViewportStyle::BorderColor().ToImU32(), 0.0f, 0, 1.0f);
	}

} // namespace Lunex