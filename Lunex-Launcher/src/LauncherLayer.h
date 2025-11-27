#pragma once

#include <Lunex.h>
#include "Panels/ProjectCreationDialog.h"

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
		void ShowProjectsPanel();
		void ShowOptionsPanel();
		void OpenProjectDialog();
		void CreateNewProjectDialog();
		void OnProjectCreated(const std::string& projectName, const std::filesystem::path& projectPath);

	private:
		bool m_ShowProjects = true;
		bool m_ShowAboutDialog = false;
		std::string m_SelectedProject = "";
		std::vector<std::string> m_RecentProjects;
		
		Ref<Texture2D> m_LogoTexture;
		Ref<Texture2D> m_IconEditor;
		
		ProjectCreationDialog m_ProjectCreationDialog;
	};

}
