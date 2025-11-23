#pragma once

#include "Core/Core.h"
#include <string>
#include <filesystem>
#include <functional>

namespace Lunex {
	class ProjectCreationDialog {
	public:
		ProjectCreationDialog();
		
		void Open();
		void Close();
		bool IsOpen() const { return m_IsOpen; }
		
		void OnImGuiRender();
		
		void SetOnCreateCallback(const std::function<void(const std::string&, const std::filesystem::path&)>& callback) {
			m_OnCreateCallback = callback;
		}
		
	private:
		void BrowseProjectLocation();
		void CreateProject();
		bool ValidateInput();
		
	private:
		bool m_IsOpen = false;
		
		// Project configuration
		char m_ProjectName[256] = "NewProject";
		char m_ProjectLocation[512] = "";
		
		// Project settings
		int m_WindowWidth = 1920;
		int m_WindowHeight = 1080;
		bool m_VSync = true;
		bool m_Fullscreen = false;
		
		// Template selection
		int m_SelectedTemplate = 0;
		const char* m_Templates[3] = { "Empty", "3D Scene", "2D Scene" };
		
		// Validation
		std::string m_ErrorMessage;
		
		// Callback
		std::function<void(const std::string&, const std::filesystem::path&)> m_OnCreateCallback;
	};
}
