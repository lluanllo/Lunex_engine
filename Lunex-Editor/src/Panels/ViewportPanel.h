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

		void OnImGuiRender(Ref<Framebuffer> framebuffer, SceneHierarchyPanel& hierarchyPanel, 
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

	private:
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];

		std::function<void(const std::filesystem::path&)> m_OnSceneDropCallback;
	};
}
