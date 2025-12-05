#include "stpch.h"
#include "ProjectManager.h"
#include "ProjectSerializer.h"

#include "Log/Log.h"
#include <filesystem>

namespace Lunex {
	Ref<Project> ProjectManager::New() {
		Project::s_ActiveProject = CreateRef<Project>();
		return Project::s_ActiveProject;
	}
	
	Ref<Project> ProjectManager::Load(const std::filesystem::path& path) {
		Ref<Project> project = CreateRef<Project>();
		
		ProjectSerializer serializer(project);
		if (!serializer.Deserialize(path)) {
			LNX_LOG_ERROR("Failed to load project: {0}", path.string());
			return nullptr;
		}
		
		project->m_ProjectPath = path;
		project->m_ProjectDirectory = path.parent_path();
		project->m_AssetDirectory = project->m_ProjectDirectory / project->m_Config.AssetDirectory;
		
		// Verify project structure
		if (!std::filesystem::exists(project->m_AssetDirectory)) {
			LNX_LOG_WARN("Asset directory not found, creating: {0}", project->m_AssetDirectory.string());
			CreateProjectDirectories(project->m_ProjectDirectory);
		}
		
		Project::s_ActiveProject = project;
		LNX_LOG_INFO("Project loaded: {0}", project->m_Config.Name);
		LNX_LOG_INFO("  Project Directory: {0}", project->m_ProjectDirectory.string());
		LNX_LOG_INFO("  Asset Directory: {0}", project->m_AssetDirectory.string());
		
		return Project::s_ActiveProject;
	}
	
	bool ProjectManager::SaveActive(const std::filesystem::path& path) {
		if (!Project::s_ActiveProject) {
			LNX_LOG_ERROR("No active project to save!");
			return false;
		}
		
		// Update project paths
		Project::s_ActiveProject->m_ProjectPath = path;
		Project::s_ActiveProject->m_ProjectDirectory = path.parent_path();
		
		// Create project directories if they don't exist
		CreateProjectDirectories(Project::s_ActiveProject->m_ProjectDirectory);
		
		// Update asset directory path in project
		Project::s_ActiveProject->m_AssetDirectory = 
			Project::s_ActiveProject->m_ProjectDirectory / Project::s_ActiveProject->m_Config.AssetDirectory;
		
		// Create default scene if it doesn't exist
		CreateDefaultScene(Project::s_ActiveProject->m_ProjectDirectory);
		
		ProjectSerializer serializer(Project::s_ActiveProject);
		if (!serializer.Serialize(path)) {
			LNX_LOG_ERROR("Failed to save project: {0}", path.string());
			return false;
		}
		
		LNX_LOG_INFO("Project saved successfully: {0}", path.string());
		return true;
	}
	
	bool ProjectManager::CreateProjectDirectories(const std::filesystem::path& projectPath) {
		// Create project root
		if (!std::filesystem::exists(projectPath)) {
			std::filesystem::create_directories(projectPath);
		}
		
		// Create standard Unity-like folder structure
		std::filesystem::path assetsPath = projectPath / "Assets";
		std::filesystem::create_directories(assetsPath);
		std::filesystem::create_directories(assetsPath / "Scenes");
		std::filesystem::create_directories(assetsPath / "Scripts");
		std::filesystem::create_directories(assetsPath / "Textures");
		std::filesystem::create_directories(assetsPath / "Models");
		std::filesystem::create_directories(assetsPath / "Materials");
		std::filesystem::create_directories(assetsPath / "MeshAssets");  // For .lumesh files
		std::filesystem::create_directories(assetsPath / "Shaders");
		std::filesystem::create_directories(assetsPath / "Audio");
		std::filesystem::create_directories(assetsPath / "Prefabs");
		
		LNX_LOG_INFO("Project directories created at: {0}", projectPath.string());
		return true;
	}

	void ProjectManager::CreateDefaultScene(const std::filesystem::path& projectPath) {
		std::filesystem::path scenePath = projectPath / "Assets" / "Scenes" / "SampleScene.lunex";
		
		// Don't overwrite if scene already exists
		if (std::filesystem::exists(scenePath))
			return;
		
		// Create default scene YAML content
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
		
		// Write scene file
		std::ofstream sceneFile(scenePath);
		sceneFile << sceneContent;
		sceneFile.close();
		
		// Update project config to use this scene as start scene
		if (Project::s_ActiveProject) {
			Project::s_ActiveProject->m_Config.StartScene = "Scenes/SampleScene.lunex";
		}
		
		LNX_LOG_INFO("Default scene created: {0}", scenePath.string());
	}

} // namespace Lunex
