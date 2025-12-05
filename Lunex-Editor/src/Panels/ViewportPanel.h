#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"
#include "ToolbarPanel.h"
#include <glm/glm.hpp>
#include <functional>
#include <filesystem>

namespace Lunex {
	class ViewportPanel {
	public:
		ViewportPanel() = default;
		~ViewportPanel() = default;

		void OnImGuiRender(Ref<Framebuffer> framebuffer, Ref<Framebuffer> cameraPreviewFramebuffer,
			SceneHierarchyPanel& hierarchyPanel,
			const EditorCamera& editorCamera, Entity selectedEntity, int gizmoType,
			ToolbarPanel& toolbarPanel, SceneState sceneState, bool toolbarEnabled);

		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }
		glm::vec2 GetViewportSize() const { return m_ViewportSize; }
		const glm::vec2* GetViewportBounds() const { return m_ViewportBounds; }

		// Callback for scene file drop
		void SetOnSceneDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnSceneDropCallback = callback;
		}
		
		// Callback for 3D model file drop (triggers import modal)
		void SetOnModelDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnModelDropCallback = callback;
		}
		
		// Callback for .lumesh file drop (creates entity directly)
		void SetOnMeshAssetDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnMeshAssetDropCallback = callback;
		}
		
		// Callback for .luprefab file drop (instantiates prefab)
		void SetOnPrefabDropCallback(std::function<void(const std::filesystem::path&)> callback) {
			m_OnPrefabDropCallback = callback;
		}

	private:
		void RenderCameraPreview(Ref<Framebuffer> previewFramebuffer, Entity selectedEntity);
		
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];

		std::function<void(const std::filesystem::path&)> m_OnSceneDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnModelDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnMeshAssetDropCallback;
		std::function<void(const std::filesystem::path&)> m_OnPrefabDropCallback;
	};
}