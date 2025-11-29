#pragma once

#include <Lunex.h>
#include "../Panels/SceneHierarchyPanel.h"
#include "../Panels/PropertiesPanel.h"
#include "../Panels/ContentBrowserPanel.h"
#include "../Panels/StatsPanel.h"
#include "../Panels/SettingsPanel.h"
#include "../Panels/ViewportPanel.h"
#include "../Panels/ToolbarPanel.h"
#include "../Panels/ConsolePanel.h"
#include "../Panels/MenuBarPanel.h"
#include "../Panels/ProjectCreationDialog.h"

#include "Renderer/EditorCamera.h"

namespace Lunex {
	class EditorLayer : public Layer {
		public:
			EditorLayer();
			virtual ~EditorLayer() = default;
			
			virtual void OnAttach() override;
			virtual void OnDetach() override;
			
			void OnUpdate(Timestep ts) override;
			virtual void OnImGuiRender() override;
			void OnEvent(Event& e) override;
			
		private:
			bool OnKeyPressed(KeyPressedEvent& e);
			bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
			
			void OnOverlayRender();
			
			// Project management
			void NewProject();
			void CreateProjectWithDialog(const std::string& name, const std::filesystem::path& location);
			void OpenProject();
			void OpenProject(const std::filesystem::path& path);
			void SaveProject();
			void SaveProjectAs();
			
			// Scene management
			void NewScene();
			void OpenScene();
			void OpenScene(const std::filesystem::path& path);
			void SaveScene();
			void SaveSceneAs();
			
			void SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);
			
			void OnScenePlay();
			void OnSceneSimulate();  
			void OnSceneStop();
			
			void OnDuplicateEntity();
			
			// UI Refresh
			void UI_UpdateWindowTitle();
			
		private:
			OrthographicCameraController m_CameraController;
			
			// Temp
			Ref<VertexArray> m_SquareVA;
			Ref<Shader> m_FlatColorShader;
			Ref<Framebuffer> m_Framebuffer;
			
			Ref<Scene> m_ActiveScene;
			Ref<Scene> m_EditorScene;
			std::filesystem::path m_EditorScenePath;
			Entity m_SquareEntity;
			Entity m_CameraEntity;
			Entity m_SecondCamera;
			Entity m_HoveredEntity;
			
			bool m_PrimaryCamera = true;
			
			EditorCamera m_EditorCamera;
			
			Ref<Texture2D> m_CheckerboardTexture;
			
			glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
			glm::vec2 m_ViewportBounds[2];
			
			glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
			
			int m_GizmoType = -1;
			
			SceneState m_SceneState = SceneState::Edit;
			
			// Panels
			SceneHierarchyPanel m_SceneHierarchyPanel;
			PropertiesPanel m_PropertiesPanel;
			ContentBrowserPanel m_ContentBrowserPanel;
			StatsPanel m_StatsPanel;
			SettingsPanel m_SettingsPanel;
			ViewportPanel m_ViewportPanel;
			ToolbarPanel m_ToolbarPanel;
			ConsolePanel m_ConsolePanel;
			MenuBarPanel m_MenuBarPanel;
			ProjectCreationDialog m_ProjectCreationDialog;
			
			// Editor resources
			Ref<Texture2D> m_IconPlay, m_IconSimulate, m_IconStop;
			
		private:
			std::string m_InitialScenePath;
	};
}