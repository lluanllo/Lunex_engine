#pragma once

#include "Core/Core.h"
#include <string>
#include <filesystem>

namespace Lunex {
	struct ProjectConfig {
		std::string Name = "Untitled";
		
		std::filesystem::path StartScene;
		std::filesystem::path AssetDirectory;
		std::filesystem::path ScriptModulePath;
		
		// Project settings
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool VSync = true;
		
		// Serialization version
		uint32_t Version = 1;
	};
	
	class Project {
	public:
		Project() = default;
		~Project() = default;
		
		const std::string& GetName() const { return m_Config.Name; }
		void SetName(const std::string& name) { m_Config.Name = name; }
		
		const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
		const std::filesystem::path& GetAssetDirectory() const { return m_AssetDirectory; }
		const std::filesystem::path& GetProjectPath() const { return m_ProjectPath; }
		
		std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path) const;
		std::filesystem::path GetAssetRelativePath(const std::filesystem::path& path) const;
		
		ProjectConfig& GetConfig() { return m_Config; }
		const ProjectConfig& GetConfig() const { return m_Config; }
		
		static const std::filesystem::path& GetActiveProjectDirectory() {
			LNX_CORE_ASSERT(s_ActiveProject, "No active project!");
			return s_ActiveProject->GetProjectDirectory();
		}
		
		static const std::filesystem::path& GetActiveAssetDirectory() {
			LNX_CORE_ASSERT(s_ActiveProject, "No active project!");
			return s_ActiveProject->GetAssetDirectory();
		}
		
		static Ref<Project> GetActive() { return s_ActiveProject; }
		
	private:
		ProjectConfig m_Config;
		
		std::filesystem::path m_ProjectDirectory;
		std::filesystem::path m_AssetDirectory;
		std::filesystem::path m_ProjectPath;
		
		inline static Ref<Project> s_ActiveProject;
		
		friend class ProjectSerializer;
		friend class ProjectManager;
	};
}
