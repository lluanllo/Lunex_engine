#pragma once

#include "Project.h"

namespace Lunex {
	// Forward declaration
	class AssetDatabase;
	
	class ProjectManager {
	public:
		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool SaveActive(const std::filesystem::path& path);
		
		static Ref<Project> GetActiveProject() { return Project::s_ActiveProject; }
		
		// ? NEW: AssetDatabase management
		static AssetDatabase& GetAssetDatabase();
		static void RefreshAssetDatabase();
		
	private:
		static bool CreateProjectDirectories(const std::filesystem::path& projectPath);
		static void CreateDefaultScene(const std::filesystem::path& projectPath);
	};
}
