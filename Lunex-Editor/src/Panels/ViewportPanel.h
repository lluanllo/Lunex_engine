#pragma once
#include "Core/Core.h"

#include "Renderer/Buffer/Framebuffer.h"
#include "Scene/Entity.h"
#include "Renderer/CameraTypes/EditorCamera.h"

#include <glm/glm.hpp>
#include <functional>
#include <filesystem>

namespace Lunex {
	class Scene;
	
	struct ViewportPanelCallbacks {
		std::function<void(const std::filesystem::path&)> OnSceneDropped;
		std::function<Entity()> GetSelectedEntity;
		std::function<const EditorCamera& ()> GetEditorCamera;
	};
	
	class ViewportPanel {
	public:
		ViewportPanel() = default;
		~ViewportPanel() = default;
		
		void OnImGuiRender();
		
		void SetFramebuffer(Ref<Framebuffer> framebuffer) { m_Framebuffer = framebuffer; }
		void SetCallbacks(const ViewportPanelCallbacks& callbacks) { m_Callbacks = callbacks; }
		void SetGizmoType(int gizmoType) { m_GizmoType = gizmoType; }
		void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }
		
		glm::vec2 GetViewportSize() const { return m_ViewportSize; }
		glm::vec2* GetViewportBounds() { return m_ViewportBounds; }
		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }
		
	private:
		void RenderGizmos(Entity selectedEntity);
		
	private:
		Ref<Framebuffer> m_Framebuffer;
		Ref<Scene> m_ActiveScene;
		ViewportPanelCallbacks m_Callbacks;
		
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2] = { {0.0f, 0.0f}, {0.0f, 0.0f} };
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		
		int m_GizmoType = -1;
	};
}