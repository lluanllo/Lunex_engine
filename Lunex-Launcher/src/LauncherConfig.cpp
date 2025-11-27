#include "LauncherConfig.h"
#include <fstream>
#include <algorithm>

namespace Lunex {

	LauncherConfig LauncherConfigManager::s_Config;

	void LauncherConfigManager::Init()
	{
		LoadConfig();
	}

	void LauncherConfigManager::Shutdown()
	{
		SaveConfig();
	}

	void LauncherConfigManager::LoadConfig()
	{
		std::string configPath = GetConfigPath();
		
		// TODO: Implement proper serialization with YAML
		// For now, just initialize with default values
		s_Config = LauncherConfig();
		
		// Check if config file exists
		if (!std::filesystem::exists(configPath))
		{
			LNX_LOG_WARN("Launcher config not found, using defaults");
			return;
		}

		// TODO: Load from file
		// This is a placeholder - implement YAML parsing
		LNX_LOG_INFO("Loading launcher configuration from: {0}", configPath);
	}

	void LauncherConfigManager::SaveConfig()
	{
		std::string configPath = GetConfigPath();
		
		// Create directory if it doesn't exist
		std::filesystem::path configDir = std::filesystem::path(configPath).parent_path();
		if (!std::filesystem::exists(configDir))
		{
			std::filesystem::create_directories(configDir);
		}

		// TODO: Implement proper serialization with YAML
		// This is a placeholder
		LNX_LOG_INFO("Saving launcher configuration to: {0}", configPath);
	}

	void LauncherConfigManager::AddRecentProject(const std::string& projectPath)
	{
		// Remove if already exists
		auto& projects = s_Config.RecentProjects;
		projects.erase(std::remove(projects.begin(), projects.end(), projectPath), projects.end());
		
		// Add to front
		projects.insert(projects.begin(), projectPath);
		
		// Limit to max recent projects
		if (projects.size() > s_MaxRecentProjects)
		{
			projects.resize(s_MaxRecentProjects);
		}
		
		s_Config.LastOpenedProject = projectPath;
		SaveConfig();
	}

	void LauncherConfigManager::RemoveRecentProject(const std::string& projectPath)
	{
		auto& projects = s_Config.RecentProjects;
		projects.erase(std::remove(projects.begin(), projects.end(), projectPath), projects.end());
		SaveConfig();
	}

	void LauncherConfigManager::ClearRecentProjects()
	{
		s_Config.RecentProjects.clear();
		s_Config.LastOpenedProject.clear();
		SaveConfig();
	}

	std::string LauncherConfigManager::GetConfigPath()
	{
		// Store config in AppData/Local/LunexEngine
		char* appDataPath;
		size_t len;
		_dupenv_s(&appDataPath, &len, "LOCALAPPDATA");
		
		if (appDataPath)
		{
			std::string configPath = std::string(appDataPath) + "/LunexEngine/launcher.config";
			free(appDataPath);
			return configPath;
		}
		
		// Fallback to current directory
		return "launcher.config";
	}

}
