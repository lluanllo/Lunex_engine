#pragma once

#include <Lunex.h>
#include "../Panels/SceneHierarchyPanel.h"
#include "../Panels/PropertiesPanel.h"
#include "../Panels/ContentBrowserPanel.h"
#include "../Panels/MaterialEditorPanel.h"
#include "../Panels/StatsPanel.h"
#include "../Panels/SettingsPanel.h"
#include "../Panels/ViewportPanel.h"
#include "../Panels/ToolbarPanel.h"  // This includes SceneMode.h and defines SceneState alias
#include "../Panels/ConsolePanel.h"
#include "../Panels/MenuBarPanel.h"
#include "../Panels/ProjectCreationDialog.h"
#include "../Panels/InputSettingsPanel.h"
#include "../Panels/GizmoSettingsPanel.h"
#include "../Panels/JobSystemPanel.h"
#include "../Panels/MeshImportModal.h"
#include "../Panels/AnimationEditorPanel.h"

#include "Scene/Camera/EditorCamera.h"
// Note: SceneMode.h is included via ToolbarPanel.h

namespace Lunex {
	// Note: SceneState alias is defined in ToolbarPanel.h
	
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
			bool OnKeyReleased(KeyReleasedEvent& e);
			bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
			
			void OnOverlayRender();
			
			// ========================================
			// Input Actions Registration
			// ========================================
			void RegisterEditorActions();
			
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
			
			// Camera Preview
			void RenderCameraPreview(Entity cameraEntity);
			
			// Mesh Import
			void OnModelDropped(const std::filesystem::path& modelPath);
			void OnMeshAssetDropped(const std::filesystem::path& meshAssetPath);
			void OnMeshImported(Ref<MeshAsset> meshAsset);
			
			// Prefab
			void OnPrefabDropped(const std::filesystem::path& prefabPath);
			
		private:
			OrthographicCameraController m_CameraController;
			
			// Temp
			Ref<VertexArray> m_SquareVA;
			Ref<Shader> m_FlatColorShader;
			Ref<Framebuffer> m_Framebuffer;
			Ref<Framebuffer> m_CameraPreviewFramebuffer;
			
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
			MaterialEditorPanel m_MaterialEditorPanel;
			StatsPanel m_StatsPanel;
			SettingsPanel m_SettingsPanel;
			ViewportPanel m_ViewportPanel;
			ToolbarPanel m_ToolbarPanel;
			ConsolePanel m_ConsolePanel;
			MenuBarPanel m_MenuBarPanel;
			ProjectCreationDialog m_ProjectCreationDialog;
			InputSettingsPanel m_InputSettingsPanel;
			GizmoSettingsPanel m_GizmoSettingsPanel;
			JobSystemPanel m_JobSystemPanel;
			MeshImportModal m_MeshImportModal;
			AnimationEditorPanel m_AnimationEditorPanel;
			
			// Editor resources
			Ref<Texture2D> m_IconPlay, m_IconSimulate, m_IconStop;
			
		private:
			std::string m_InitialScenePath;
	};
}
