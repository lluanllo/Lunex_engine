#pragma once

#include <functional>
#include <string>

namespace Lunex {
	class MenuBarPanel {
	public:
		MenuBarPanel() = default;
		
		void OnImGuiRender();
		
		// Callbacks
		void SetOnNewProjectCallback(const std::function<void()>& callback) { m_OnNewProject = callback; }
		void SetOnOpenProjectCallback(const std::function<void()>& callback) { m_OnOpenProject = callback; }
		void SetOnSaveProjectCallback(const std::function<void()>& callback) { m_OnSaveProject = callback; }
		void SetOnSaveProjectAsCallback(const std::function<void()>& callback) { m_OnSaveProjectAs = callback; }
		
		void SetOnNewSceneCallback(const std::function<void()>& callback) { m_OnNewScene = callback; }
		void SetOnOpenSceneCallback(const std::function<void()>& callback) { m_OnOpenScene = callback; }
		void SetOnSaveSceneCallback(const std::function<void()>& callback) { m_OnSaveScene = callback; }
		void SetOnSaveSceneAsCallback(const std::function<void()>& callback) { m_OnSaveSceneAs = callback; }
		
		void SetOnExitCallback(const std::function<void()>& callback) { m_OnExit = callback; }
		
		void SetProjectName(const std::string& name) { m_ProjectName = name; }
		
	private:
		std::function<void()> m_OnNewProject;
		std::function<void()> m_OnOpenProject;
		std::function<void()> m_OnSaveProject;
		std::function<void()> m_OnSaveProjectAs;
		
		std::function<void()> m_OnNewScene;
		std::function<void()> m_OnOpenScene;
		std::function<void()> m_OnSaveScene;
		std::function<void()> m_OnSaveSceneAs;
		
		std::function<void()> m_OnExit;
		
		std::string m_ProjectName = "No Project";
	};
}
