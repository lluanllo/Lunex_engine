#pragma once

#include "Log/Log.h"
#include <string>
#include <vector>
#include <filesystem>

namespace Lunex {

	struct LauncherConfig
	{
		std::vector<std::string> RecentProjects;
		std::string LastOpenedProject;
		bool AutoLaunchLastProject = false;
		
		// Window settings
		int WindowWidth = 1280;
		int WindowHeight = 720;
		
		// Paths
		std::string EditorPath;
	};

	class LauncherConfigManager
	{
	public:
		static void Init();
		static void Shutdown();
		
		static void LoadConfig();
		static void SaveConfig();
		
		static LauncherConfig& GetConfig() { return s_Config; }
		
		static void AddRecentProject(const std::string& projectPath);
		static void RemoveRecentProject(const std::string& projectPath);
		static void ClearRecentProjects();
		
		static std::string GetConfigPath();
		
	private:
		static LauncherConfig s_Config;
		static const size_t s_MaxRecentProjects = 10;
	};

}
