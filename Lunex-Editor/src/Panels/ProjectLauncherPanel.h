#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Project/ProjectVersioning.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Lunex {

	/**
	 * @class ProjectLauncherPanel
	 * @brief Startup project browser (Unreal-style).
	 *
	 * Sections: Projects (recent grid with auto-captured thumbnails), New Project
	 * (inline form, no modal) and Versions (snapshots of the selected project).
	 *
	 * Shown full-window when the editor starts without a project. Recent projects are
	 * stored per user in %APPDATA%/Lunex/RecentProjects.txt (one path per line,
	 * most recent first).
	 */
	class ProjectLauncherPanel {
	public:
		struct RecentProject {
			std::filesystem::path ProjectFile;
			std::string Name;
			std::string LastOpened;   // formatted local time
			bool Exists = true;
		};

		ProjectLauncherPanel();

		enum class Section { Projects, Create, Versions };

		void Open() { m_IsOpen = true; m_Section = Section::Projects; RefreshRecentInfo(); m_Thumbnails.clear(); }
		void OpenCreate() { Open(); m_Section = Section::Create; }
		void Close() { m_IsOpen = false; }
		bool IsOpen() const { return m_IsOpen; }

		void OnImGuiRender();

		// Call whenever a project is opened/created so it shows up first next time
		void AddRecentProject(const std::filesystem::path& projectFile);

		void SetOnOpenProject(std::function<void(const std::filesystem::path&)> cb) { m_OnOpenProject = std::move(cb); }
		void SetOnBrowseProject(std::function<void()> cb) { m_OnBrowseProject = std::move(cb); }
		void SetOnCreateProject(std::function<void(const std::string&, const std::filesystem::path&)> cb) { m_OnCreateProject = std::move(cb); }

	private:
		static std::filesystem::path GetRecentFilePath();
		void LoadRecent();
		void SaveRecent() const;
		void RefreshRecentInfo();
		void RemoveRecent(size_t index);

		void DrawSidebar(float width, float height);
		void DrawProjectsSection(int& openRequest, int& removeRequest);
		void DrawCreateSection();
		void DrawVersionsSection();
		Ref<Texture2D> GetThumbnail(const std::filesystem::path& projectFile);
		void RefreshVersions();

	private:
		bool m_IsOpen = false;
		bool m_CommandLineChecked = false;   // "Lunex-Editor.exe <project.lunex>" skips the launcher
		std::vector<RecentProject> m_Recent;
		int m_Selected = -1;
		char m_Filter[128] = "";
		Section m_Section = Section::Projects;

		// New project form
		char m_NewName[64] = "NewProject";
		char m_NewLocation[512] = "";
		std::string m_CreateError;

		// Versions
		char m_SnapshotMessage[256] = "";
		std::vector<ProjectVersion> m_Versions;
		std::filesystem::path m_VersionsProject;
		std::string m_VersionStatus;

		struct ThumbnailEntry { Ref<Texture2D> Texture; std::filesystem::file_time_type Time; bool Missing = false; };
		std::map<std::string, ThumbnailEntry> m_Thumbnails;

		std::function<void(const std::filesystem::path&)> m_OnOpenProject;
		std::function<void()> m_OnBrowseProject;
		std::function<void(const std::string&, const std::filesystem::path&)> m_OnCreateProject;

		static constexpr size_t MaxRecent = 20;
	};

} // namespace Lunex
