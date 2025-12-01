#include "LauncherLayer.h"
#include "LauncherConfig.h"
#include "Core/VersionManager.h"
#include <imgui.h>
#include <filesystem>
#include <windows.h>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <commdlg.h>

namespace Lunex {

	LauncherLayer::LauncherLayer()
		: Layer("LauncherLayer")
	{
	}

	void LauncherLayer::OnAttach()
	{
		// Initialize systems
		LauncherConfigManager::Init();
		VersionManager::Init();
		
		// TODO: Cargar logo y textures si existen
		// m_LogoTexture = Texture2D::Create("assets/textures/logo.png");
		
		// Load recent projects from config
		m_RecentProjects = LauncherConfigManager::GetConfig().RecentProjects;
		
		// Setup project creation dialog callback
		m_ProjectCreationDialog.SetOnCreateCallback(
			[this](const std::string& name, const std::filesystem::path& path) {
				OnProjectCreated(name, path);
			}
		);
		
		// Check for updates and prerequisites
		CheckForUpdates();
		CheckPrerequisites();
		
		// If no recent projects, add some defaults (for testing)
		if (m_RecentProjects.empty())
		{
			LNX_LOG_INFO("No recent projects found");
		}
	}

	void LauncherLayer::OnDetach()
	{
		LauncherConfigManager::Shutdown();
	}

	void LauncherLayer::OnUpdate(Timestep ts)
	{
		// Check if editor process is running and capture logs
		if (m_EditorProcess && m_EditorProcess->IsRunning()) {
			std::string line;
			while (m_EditorProcess->ReadStdoutLine(line)) {
				m_EditorLogs.push_back(line);
				LNX_LOG_INFO("[Editor] {0}", line);
			}
		}
	}

	void LauncherLayer::OnImGuiRender()
	{
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Lunex Launcher", nullptr, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("LauncherDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		// Menu Bar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
					Application::Get().Close();
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("System Info", nullptr, &m_ShowSystemInfo);
				ImGui::MenuItem("Logs", nullptr, &m_ShowLogs);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("Check for Updates"))
				{
					CheckForUpdates();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("About"))
				{
					m_ShowAboutDialog = true;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// Main Content
		ImGui::Begin("Lunex Engine Launcher");

		// Header with version
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::Text("LUNEX ENGINE");
		ImGui::PopFont();
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "v%s", VersionManager::GetCurrentVersion().ToString().c_str());
		
		ImGui::Text("Game Engine Launcher");
		
		// Show update notification
		if (m_UpdateAvailable) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Update Available!");
		}
		
		// Show missing prerequisites warning
		if (!m_MissingPrerequisites.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: Missing Prerequisites!");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				for (const auto& prereq : m_MissingPrerequisites) {
					ImGui::Text("- %s", prereq.c_str());
				}
				ImGui::EndTooltip();
			}
		}
		
		ImGui::Separator();
		ImGui::Spacing();

		// Launch Editor Button
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
		
		ImVec2 buttonSize = ImVec2(200, 50);
		if (ImGui::Button("Launch Editor", buttonSize))
		{
			LaunchEditor();
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(20, 0));
		ImGui::SameLine();
		
		if (ImGui::Button("Launch with Logs", buttonSize))
		{
			LaunchEditorWithLogs();
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(20, 0));
		ImGui::SameLine();

		if (ImGui::Button("Open Project", buttonSize))
		{
			OpenProjectDialog();
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(20, 0));
		ImGui::SameLine();

		if (ImGui::Button("New Project", buttonSize))
		{
			CreateNewProjectDialog();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Recent Projects
		ImGui::Text("Recent Projects:");
		ImGui::Spacing();

		if (m_RecentProjects.empty())
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No recent projects");
		}
		else
		{
			for (size_t i = 0; i < m_RecentProjects.size(); i++)
			{
				const auto& project = m_RecentProjects[i];
				std::string label = std::filesystem::path(project).filename().string();
				
				ImGui::PushID((int)i);
				if (ImGui::Selectable(label.c_str(), m_SelectedProject == project))
				{
					m_SelectedProject = project;
				}
				
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("%s", project.c_str());
					ImGui::EndTooltip();
				}
				ImGui::PopID();
			}
		}

		ImGui::End();

		// Options Panel
		ShowOptionsPanel();
		
		// System Info Panel
		if (m_ShowSystemInfo) {
			ShowSystemInfoPanel();
		}
		
		// Logs Panel
		if (m_ShowLogs) {
			ImGui::Begin("Editor Logs", &m_ShowLogs);
			
			if (ImGui::Button("Clear")) {
				m_EditorLogs.clear();
			}
			
			ImGui::Separator();
			
			ImGui::BeginChild("LogsScrolling");
			for (const auto& log : m_EditorLogs) {
				ImGui::TextUnformatted(log.c_str());
			}
			
			// Auto-scroll
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			
			ImGui::EndChild();
			ImGui::End();
		}
		
		// Project Creation Dialog
		m_ProjectCreationDialog.OnImGuiRender();

		ImGui::End(); // Dockspace
	}

	void LauncherLayer::OnEvent(Event& e)
	{
	}

	void LauncherLayer::LaunchEditor()
	{
		LNX_LOG_INFO("Launching Lunex Editor...");

		// Get the path to the editor executable
		std::filesystem::path launcherPath = std::filesystem::current_path();
		std::filesystem::path editorPath;

		#ifdef LN_DEBUG
			// Try relative path from bin directory
			editorPath = launcherPath.parent_path() / "Lunex-Editor" / "Lunex-Editor.exe";
			
			// If not found, try looking in the same directory level
			if (!std::filesystem::exists(editorPath))
			{
				editorPath = launcherPath / "Lunex-Editor.exe";
			}
		#else
			editorPath = launcherPath.parent_path() / "Lunex-Editor" / "Lunex-Editor.exe";
			
			if (!std::filesystem::exists(editorPath))
			{
				editorPath = launcherPath / "Lunex-Editor.exe";
			}
		#endif

		if (!std::filesystem::exists(editorPath))
		{
			LNX_LOG_ERROR("Editor executable not found at: {0}", editorPath.string());
			LNX_LOG_ERROR("Current path: {0}", launcherPath.string());
			return;
		}

		LNX_LOG_INFO("Editor found at: {0}", editorPath.string());

		// Build arguments
		std::vector<std::string> args;
		if (!m_SelectedProject.empty()) {
			args.push_back(m_SelectedProject);
			LauncherConfigManager::AddRecentProject(m_SelectedProject);
		}

		// Launch using Process class
		m_EditorProcess = std::make_unique<Process>();
		if (m_EditorProcess->Launch(editorPath.string(), args)) {
			LNX_LOG_INFO("Editor launched successfully!");
			// Close launcher after launching editor
			Application::Get().Close();
		} else {
			LNX_LOG_ERROR("Failed to launch editor");
			m_EditorProcess.reset();
		}
	}
	
	void LauncherLayer::LaunchEditorWithLogs()
	{
		LNX_LOG_INFO(" launching Lunex Editor with log capture...");

		// Get the path to the editor executable
		std::filesystem::path launcherPath = std::filesystem::current_path();
		std::filesystem::path editorPath;

		#ifdef LN_DEBUG
			editorPath = launcherPath.parent_path() / "Lunex-Editor" / "Lunex-Editor.exe";
			if (!std::filesystem::exists(editorPath))
				editorPath = launcherPath / "Lunex-Editor.exe";
		#else
			editorPath = launcherPath.parent_path() / "Lunex-Editor" / "Lunex-Editor.exe";
			if (!std::filesystem::exists(editorPath))
				editorPath = launcherPath / "Lunex-Editor.exe";
		#endif

		if (!std::filesystem::exists(editorPath))
		{
			LNX_LOG_ERROR("Editor executable not found at: {0}", editorPath.string());
			return;
		}

		// Build arguments
		std::vector<std::string> args;
		if (!m_SelectedProject.empty()) {
			args.push_back(m_SelectedProject);
			LauncherConfigManager::AddRecentProject(m_SelectedProject);
		}

		// Launch with log capture
		m_EditorProcess = std::make_unique<Process>();
		m_EditorLogs.clear();
		m_ShowLogs = true;
		
		if (m_EditorProcess->LaunchWithCapture(editorPath.string(), args)) {
			LNX_LOG_INFO("Editor launched with log capture!");
		} else {
			LNX_LOG_ERROR("Failed to launch editor");
			m_EditorProcess.reset();
		}
	}

	void LauncherLayer::ShowProjectsPanel()
	{
		// This could be expanded to show a more detailed project browser
	}

	void LauncherLayer::ShowOptionsPanel()
	{
		ImGui::Begin("Engine Info");
		
		ImGui::Text("Lunex Engine");
		ImGui::Text("Version: %s", VersionManager::GetCurrentVersion().ToString().c_str());
		ImGui::Separator();
		
		ImGui::Text("Renderer: OpenGL");
		ImGui::Text("Physics: Box2D");
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		ImGui::Text("Build Configuration:");
		#ifdef LN_DEBUG
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Debug");
		#elif defined(LN_RELEASE)
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Release");
		#else
			ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "Distribution");
		#endif
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		if (ImGui::Button("Clear Recent Projects"))
		{
			LauncherConfigManager::ClearRecentProjects();
			m_RecentProjects.clear();
			m_SelectedProject.clear();
		}
		
		ImGui::End();
	}
	
	void LauncherLayer::ShowSystemInfoPanel()
	{
		ImGui::Begin("System Information", &m_ShowSystemInfo);
		
		ImGui::Text("Prerequisites Check:");
		ImGui::Separator();
		
		if (m_MissingPrerequisites.empty()) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "All prerequisites met!");
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Missing Prerequisites:");
			for (const auto& prereq : m_MissingPrerequisites) {
				ImGui::BulletText("%s", prereq.c_str());
			}
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		if (ImGui::Button("Refresh")) {
			CheckPrerequisites();
		}
		
		ImGui::End();
	}

	void LauncherLayer::OpenProjectDialog()
	{
		// Use Windows file dialog
		OPENFILENAMEA ofn;
		char szFile[260] = { 0 };
		
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "Lunex Project\0*.lunex\0All Files\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			m_SelectedProject = ofn.lpstrFile;
			LauncherConfigManager::AddRecentProject(m_SelectedProject);
			m_RecentProjects = LauncherConfigManager::GetConfig().RecentProjects;
			LNX_LOG_INFO("Project selected: {0}", m_SelectedProject);
		}
	}

	void LauncherLayer::CreateNewProjectDialog()
	{
		m_ProjectCreationDialog.Open();
	}
	
	void LauncherLayer::OnProjectCreated(const std::string& projectName, const std::filesystem::path& projectPath)
	{
		LNX_LOG_INFO("Creating project: {0} at {1}", projectName, projectPath.string());
		
		try {
			// Create project directory
			if (!std::filesystem::exists(projectPath)) {
				std::filesystem::create_directories(projectPath);
			}
			
			// Create project structure
			std::filesystem::path assetsPath = projectPath / "Assets";
			std::filesystem::create_directories(assetsPath / "Scenes");
			std::filesystem::create_directories(assetsPath / "Scripts");
			std::filesystem::create_directories(assetsPath / "Textures");
			std::filesystem::create_directories(assetsPath / "Models");
			std::filesystem::create_directories(assetsPath / "Materials");
			std::filesystem::create_directories(assetsPath / "Shaders");
			std::filesystem::create_directories(assetsPath / "Audio");
			std::filesystem::create_directories(assetsPath / "Prefabs");
			
			// Create default scene
			std::filesystem::path scenePath = assetsPath / "Scenes" / "SampleScene.lunex";
			std::string sceneContent = R"(Scene: SampleScene
Entities:
  - Entity: 2207194862773423064
    TagComponent:
      Tag: Point Light
    TransformComponent:
      Translation: [0, 2, 2.7]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    LightComponent:
      Type: 1
      Color: [1, 1, 1]
      Intensity: 1
      Range: 10
      Attenuation: [1, 0.09, 0.032]
      InnerConeAngle: 12.5
      OuterConeAngle: 17.5
      CastShadows: true
  - Entity: 13952283562700553889
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [3.6729867, 1.5245061, 4.00342]
      Rotation: [0, 0.78539824, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 1
        PerspectiveFOV: 0.7853982
        PerspectiveNear: 0.01
        PerspectiveFar: 1000
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
  - Entity: 11697999568453830733
    TagComponent:
      Tag: Cube
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    MeshComponent:
      Type: 0
      FilePath: ""
      Color: [1, 1, 1, 1]
    MaterialComponent:
      Color: [1, 1, 1, 1]
      Metallic: 0
      Roughness: 0.5
      Specular: 0.5
      EmissionColor: [0, 0, 0]
      EmissionIntensity: 0
)";
			
			std::ofstream sceneFile(scenePath);
			sceneFile << sceneContent;
			sceneFile.close();
			
			// Create project file (.lunex)
			std::filesystem::path projectFilePath = projectPath / (projectName + ".lunex");
			
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Project" << YAML::Value;
			out << YAML::BeginMap;
			
			out << YAML::Key << "Name" << YAML::Value << projectName;
			out << YAML::Key << "Version" << YAML::Value << 1;
			out << YAML::Key << "StartScene" << YAML::Value << "Scenes/SampleScene.lunex";
			out << YAML::Key << "AssetDirectory" << YAML::Value << "Assets";
			out << YAML::Key << "ScriptModulePath" << YAML::Value << "";
			
			out << YAML::Key << "Settings" << YAML::Value;
			out << YAML::BeginMap;
			out << YAML::Key << "Width" << YAML::Value << 1920;
			out << YAML::Key << "Height" << YAML::Value << 1080;
			out << YAML::Key << "VSync" << YAML::Value << true;
			out << YAML::EndMap;
			
			out << YAML::EndMap;
			out << YAML::EndMap;
			
			std::ofstream projectFile(projectFilePath);
			projectFile << out.c_str();
			projectFile.close();
			
			// Add to recent projects
			m_SelectedProject = projectFilePath.string();
			LauncherConfigManager::AddRecentProject(m_SelectedProject);
			m_RecentProjects = LauncherConfigManager::GetConfig().RecentProjects;
			
			LNX_LOG_INFO("Project created successfully: {0}", projectFilePath.string());
			
		} catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to create project: {0}", e.what());
		}
	}
	
	void LauncherLayer::CheckForUpdates()
	{
		m_UpdateAvailable = VersionManager::IsUpdateAvailable();
		if (m_UpdateAvailable) {
			LNX_LOG_INFO("Update available! Latest version: {0}", VersionManager::GetLatestVersion().ToString());
		} else {
			LNX_LOG_INFO("You are running the latest version");
		}
	}
	
	void LauncherLayer::CheckPrerequisites()
	{
		m_MissingPrerequisites = VersionManager::GetMissingPrerequisites();
		if (m_MissingPrerequisites.empty()) {
			LNX_LOG_INFO("All prerequisites met!");
		} else {
			LNX_LOG_WARN("Missing prerequisites:");
			for (const auto& prereq : m_MissingPrerequisites) {
				LNX_LOG_WARN("  - {0}", prereq);
			}
		}
	}
}
