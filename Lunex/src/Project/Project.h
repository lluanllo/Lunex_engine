#pragma once

#include "Core/Core.h"
#include "RHI/RHITypes.h"
#include <string>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * InputBindingEntry - Stores a single key binding for serialization
	 */
	struct InputBindingEntry {
		int KeyCode = 0;
		int Modifiers = 0;
		std::string ActionName;
	};
	
	/**
	 * OutlinePreferencesConfig - Stores outline & collider visual settings
	 */
	struct OutlinePreferencesConfig {
		// Selection Outline
		glm::vec4 OutlineColor       = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		int       OutlineKernelSize  = 3;
		float     OutlineHardness    = 0.75f;
		float     OutlineInsideAlpha = 0.0f;
		bool      ShowBehindObjects  = true;
		
		// Collider Appearance
		glm::vec4 Collider2DColor    = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		glm::vec4 Collider3DColor    = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
		float     ColliderLineWidth  = 4.0f;
		
		// Gizmo Appearance (frustums, light gizmos, etc.)
		float     GizmoLineWidth     = 1.5f;
	};
	
	/**
	 * PostProcessPreferencesConfig - Stores post-processing settings for project serialization
	 */
	struct PostProcessPreferencesConfig {
		// Bloom
		bool  EnableBloom = false;
		float BloomThreshold = 1.0f;
		float BloomIntensity = 0.5f;
		float BloomRadius = 1.0f;
		int   BloomMipLevels = 5;
		
		// Vignette
		bool  EnableVignette = false;
		float VignetteIntensity = 0.3f;
		float VignetteRoundness = 1.0f;
		float VignetteSmoothness = 0.4f;
		
		// Chromatic Aberration
		bool  EnableChromaticAberration = false;
		float ChromaticAberrationIntensity = 3.0f;
		
		// Tone Mapping
		int   ToneMapOperator = 0;
		float Exposure = 1.0f;
		float Gamma = 2.2f;
	};
	
	struct ProjectConfig {
		std::string Name = "Untitled";
		
		std::filesystem::path StartScene;
		std::filesystem::path AssetDirectory;
		std::filesystem::path ScriptModulePath;
		
		// Project settings
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool VSync = true;
		
		// Rendering API
		RHI::GraphicsAPI RenderAPI = RHI::GraphicsAPI::OpenGL;
		
		// ? Input Bindings
		std::vector<InputBindingEntry> InputBindings;
		
		// ? Outline & Collider Preferences
		OutlinePreferencesConfig OutlinePreferences;
		
		// ? Post-Processing Preferences
		PostProcessPreferencesConfig PostProcessPreferences;
		
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
