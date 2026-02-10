/**
 * @file MenuBarPanel.h
 * @brief Menu Bar Panel - Main application menu bar
 * 
 * Features:
 * - File menu (Project & Scene operations)
 * - Edit menu (Undo/Redo, Clipboard)
 * - View menu (Panel visibility)
 * - Preferences menu (Settings)
 * - Help menu
 * - Logo display
 * - Project/Scene name display
 */

#pragma once

#include "Renderer/Texture.h"
#include "Core/Core.h"

#include <functional>
#include <string>

namespace Lunex {

	class MenuBarPanel {
	public:
		MenuBarPanel() = default;
		
		void OnImGuiRender();
		
		// Project callbacks
		void SetOnNewProjectCallback(const std::function<void()>& callback) { m_OnNewProject = callback; }
		void SetOnOpenProjectCallback(const std::function<void()>& callback) { m_OnOpenProject = callback; }
		void SetOnSaveProjectCallback(const std::function<void()>& callback) { m_OnSaveProject = callback; }
		void SetOnSaveProjectAsCallback(const std::function<void()>& callback) { m_OnSaveProjectAs = callback; }
		
		// Scene callbacks
		void SetOnNewSceneCallback(const std::function<void()>& callback) { m_OnNewScene = callback; }
		void SetOnOpenSceneCallback(const std::function<void()>& callback) { m_OnOpenScene = callback; }
		void SetOnSaveSceneCallback(const std::function<void()>& callback) { m_OnSaveScene = callback; }
		void SetOnSaveSceneAsCallback(const std::function<void()>& callback) { m_OnSaveSceneAs = callback; }
		
		// Other callbacks
		void SetOnExitCallback(const std::function<void()>& callback) { m_OnExit = callback; }
		void SetOnOpenInputSettingsCallback(const std::function<void()>& callback) { m_OnOpenInputSettings = callback; }
		void SetOnOpenJobSystemPanelCallback(const std::function<void()>& callback) { m_OnOpenJobSystemPanel = callback; }
		
		// Display info
		void SetProjectName(const std::string& name) { m_ProjectName = name; }
		void SetSceneName(const std::string& name) { m_SceneName = name; }
		
	private:
		// Render helpers
		void RenderLogo();
		void RenderFileMenu();
		void RenderEditMenu();
		void RenderViewMenu();
		void RenderPreferencesMenu();
		void RenderHelpMenu();
		void RenderStatusInfo();

	private:
		// Project callbacks
		std::function<void()> m_OnNewProject;
		std::function<void()> m_OnOpenProject;
		std::function<void()> m_OnSaveProject;
		std::function<void()> m_OnSaveProjectAs;
		
		// Scene callbacks
		std::function<void()> m_OnNewScene;
		std::function<void()> m_OnOpenScene;
		std::function<void()> m_OnSaveScene;
		std::function<void()> m_OnSaveSceneAs;
		
		// Other callbacks
		std::function<void()> m_OnExit;
		std::function<void()> m_OnOpenInputSettings;
		std::function<void()> m_OnOpenJobSystemPanel;
		
		// Display info
		std::string m_ProjectName = "No Project";
		std::string m_SceneName = "Untitled";
		Ref<Texture2D> m_LogoTexture;
	};

} // namespace Lunex
