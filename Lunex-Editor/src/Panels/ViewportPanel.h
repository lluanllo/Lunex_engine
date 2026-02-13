/**
 * @file ViewportPanel.h
 * @brief Viewport Panel - Main 3D/2D scene viewport with gizmos
 * 
 * Features:
 * - Main scene framebuffer display
 * - ImGuizmo transform manipulation
 * - Multi-selection gizmo with pivot point modes
 * - Drag & drop for scenes, models, prefabs
 * - Camera preview overlay for selected cameras
 * - Toolbar integration
 */

#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Scene/Camera/EditorCamera.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"
#include "GizmoSettingsPanel.h"
#include "ToolbarPanel.h"

#include <glm/glm.hpp>
#include <functional>
#include <filesystem>
#include <set>
#include <unordered_map>

namespace Lunex {

	class ViewportPanel {
	public:
		ViewportPanel() = default;
		~ViewportPanel() = default;

		void OnImGuiRender(
			Ref<Framebuffer> framebuffer, 
			Ref<Framebuffer> cameraPreviewFramebuffer,
			SceneHierarchyPanel& hierarchyPanel,
			const EditorCamera& editorCamera, 
			Entity selectedEntity, 
			int gizmoType,
			ToolbarPanel& toolbarPanel, 
			SceneState sceneState, 
			bool toolbarEnabled,
			GizmoSettingsPanel& gizmoSettings
		);

		// State queries
		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }
		glm::vec2 GetViewportSize() const { return m_ViewportSize; }
		const glm::vec2* GetViewportBounds() const { return m_ViewportBounds; }

		/** Set an override texture ID for the viewport (e.g. path tracer output).
		 *  Pass 0 to revert to the default framebuffer. */
		void SetOverrideTextureID(uint32_t texID) { m_OverrideTextureID = texID; }

		// Drag & drop callbacks
		void SetOnSceneDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnSceneDropCallback = callback;
		}
		
		void SetOnModelDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnModelDropCallback = callback;
		}
		
		void SetOnMeshAssetDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnMeshAssetDropCallback = callback;
		}
		
		void SetOnPrefabDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnPrefabDropCallback = callback;
		}

	private:
		// Render helpers
		void RenderFramebufferImage(Ref<Framebuffer> framebuffer);
		void HandleDragDrop();
		void RenderGizmos(
			SceneHierarchyPanel& hierarchyPanel,
			const EditorCamera& editorCamera,
			int gizmoType,
			GizmoSettingsPanel& gizmoSettings
		);
		void RenderCameraPreview(Ref<Framebuffer> previewFramebuffer, Entity selectedEntity);
		
	private:
		// Viewport state
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];

		// Override texture for path tracer output (0 = use framebuffer)
		uint32_t m_OverrideTextureID = 0;

		// Gizmo state (for multi-selection delta tracking)
		bool m_GizmoWasUsing = false;
		glm::mat4 m_GizmoPreviousTransform = glm::mat4(1.0f);
		glm::vec3 m_GizmoCachedPivot = glm::vec3(0.0f);
		
		// Per-entity initial state cached at drag start (for rotation stability)
		glm::mat4 m_GizmoInitialPivotTransform = glm::mat4(1.0f);
		std::unordered_map<entt::entity, glm::vec3> m_GizmoInitialEntityTranslations;
		std::unordered_map<entt::entity, glm::vec3> m_GizmoInitialEntityRotations;

		// Callbacks
		std::function<void(const std::filesystem::path&)> m_OnSceneDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnModelDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnMeshAssetDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnPrefabDropCallback;
	};

} // namespace Lunex
