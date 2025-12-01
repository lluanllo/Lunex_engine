#pragma once

#include <Lunex.h>
#include "Panels/ProjectCreationDialog.h"
#include "Core/Process.h"

namespace Lunex {

	class LauncherLayer : public Layer {
	public:
		LauncherLayer();
		virtual ~LauncherLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		void LaunchEditor();
		void LaunchEditorWithLogs();
		void ShowProjectsPanel();
		void ShowOptionsPanel();
		void ShowSystemInfoPanel();
		void OpenProjectDialog();
		void CreateNewProjectDialog();
		void OnProjectCreated(const std::string& projectName, const std::filesystem::path& projectPath);
		
		void CheckForUpdates();
		void CheckPrerequisites();

	private:
		bool m_ShowProjects = true;
		bool m_ShowAboutDialog = false;
		bool m_ShowSystemInfo = false;
		bool m_ShowLogs = false;
		std::string m_SelectedProject = "";
		std::vector<std::string> m_RecentProjects;
		
		// Version and update info
		bool m_UpdateAvailable = false;
		std::vector<std::string> m_MissingPrerequisites;
		
		// Process management
		std::unique_ptr<Process> m_EditorProcess;
		std::vector<std::string> m_EditorLogs;
		
		Ref<Texture2D> m_LogoTexture;
		Ref<Texture2D> m_IconEditor;
		
		ProjectCreationDialog m_ProjectCreationDialog;
	};

}
