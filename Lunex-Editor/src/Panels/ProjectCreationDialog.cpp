#include "stpch.h"
#include "ProjectCreationDialog.h"
#include "Utils/PlatformUtils.h"

#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

namespace Lunex {
	ProjectCreationDialog::ProjectCreationDialog() {
		// Set default project location to Documents/LunexProjects
		std::filesystem::path documentsPath = std::filesystem::path(getenv("USERPROFILE")) / "Documents" / "LunexProjects";
		strncpy_s(m_ProjectLocation, sizeof(m_ProjectLocation), documentsPath.string().c_str(), _TRUNCATE);
	}
	
	void ProjectCreationDialog::Open() {
		m_IsOpen = true;
		m_ErrorMessage.clear();
	}
	
	void ProjectCreationDialog::Close() {
		m_IsOpen = false;
		m_ErrorMessage.clear();
	}
	
	void ProjectCreationDialog::OnImGuiRender() {
		if (!m_IsOpen)
			return;
		
		using namespace UI;
		
		OpenPopup("Create New Project");
		
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Appearing);
		
		if (BeginModal("Create New Project", &m_IsOpen, Size(600, 450), ImGuiWindowFlags_NoResize)) {
			ScopedStyle dialogStyles(
				ImGuiStyleVar_FramePadding, ImVec2(8, 6),
				ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));
			
			// Header
			{
				ScopedColor headerColor(ImGuiCol_Text, Color(0.9f, 0.9f, 0.9f, 1.0f));
				Heading("Create New Lunex Project", 1);
			}
			
			Separator();
			AddSpacing(SpacingValues::SM);
			
			// Project Name
			Text("Project Name");
			ImGui::SetNextItemWidth(-1);
			InputText("##ProjectName", m_ProjectName, sizeof(m_ProjectName));
			
			AddSpacing(SpacingValues::SM);
			
			// Project Location
			Text("Project Location");
			ImGui::SetNextItemWidth(-80);
			InputText("##ProjectLocation", m_ProjectLocation, sizeof(m_ProjectLocation));
			SameLine();
			if (Button("Browse...", ButtonVariant::Default, ButtonSize::Medium, Size(70, 0))) {
				BrowseProjectLocation();
			}
			
			// Show full path preview
			std::filesystem::path fullPath = std::filesystem::path(m_ProjectLocation) / m_ProjectName;
			TextColored(Color(0.6f, 0.6f, 0.6f, 1.0f), "Project will be created at: %s", fullPath.string().c_str());
			
			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::SM);
			
			// Template Selection
			Text("Template");
			ImGui::SetNextItemWidth(-1);
			ImGui::Combo("##Template", &m_SelectedTemplate, m_Templates, IM_ARRAYSIZE(m_Templates));
			
			// Template description
			{
				ScopedColor descColor(ImGuiCol_Text, Color(0.7f, 0.7f, 0.7f, 1.0f));
				switch (m_SelectedTemplate) {
				case 0:
					TextWrapped("Empty project with basic folder structure");
					break;
				case 1:
					TextWrapped("3D project with camera, light, and cube");
					break;
				case 2:
					TextWrapped("2D project with orthographic camera and sprite");
					break;
				}
			}
			
			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::SM);
			
			// Project Settings
			if (BeginSection("Project Settings", true)) {
				Text("Window Size");
				ImGui::SetNextItemWidth(150);
				InputInt("##WindowWidth", m_WindowWidth);
				SameLine();
				ImGui::SetNextItemWidth(150);
				InputInt("##WindowHeight", m_WindowHeight);
				
				AddSpacing(SpacingValues::SM);
				Checkbox("VSync", m_VSync);
				SameLine();
				Checkbox("Fullscreen", m_Fullscreen);
				
				EndSection();
			}
			
			AddSpacing(SpacingValues::SM);
			
			// Error message
			if (!m_ErrorMessage.empty()) {
				TextColored(Color(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_ErrorMessage.c_str());
				AddSpacing(SpacingValues::SM);
			}
			
			// Buttons
			Separator();
			AddSpacing(SpacingValues::SM);
			
			float buttonWidth = 120.0f;
			float spacing = 10.0f;
			float totalWidth = (buttonWidth * 2) + spacing;
			float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
			
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			
			if (Button("Create", ButtonVariant::Primary, ButtonSize::Large, Size(buttonWidth, 35))) {
				if (ValidateInput()) {
					CreateProject();
					Close();
				}
			}
			
			SameLine(0, spacing);
			
			if (Button("Cancel", ButtonVariant::Default, ButtonSize::Large, Size(buttonWidth, 35))) {
				Close();
			}
			
			EndModal();
		}
	}
	
	void ProjectCreationDialog::BrowseProjectLocation() {
		std::string folder = FileDialogs::SelectFolder();
		if (!folder.empty()) {
			strncpy_s(m_ProjectLocation, sizeof(m_ProjectLocation), folder.c_str(), _TRUNCATE);
		}
	}
	
	void ProjectCreationDialog::CreateProject() {
		std::filesystem::path projectPath = std::filesystem::path(m_ProjectLocation) / m_ProjectName;
		
		if (m_OnCreateCallback) {
			m_OnCreateCallback(m_ProjectName, projectPath);
		}
	}
	
	bool ProjectCreationDialog::ValidateInput() {
		m_ErrorMessage.clear();
		
		// Validate project name
		if (strlen(m_ProjectName) == 0) {
			m_ErrorMessage = "Project name cannot be empty";
			return false;
		}
		
		// Validate project location
		if (strlen(m_ProjectLocation) == 0) {
			m_ErrorMessage = "Project location cannot be empty";
			return false;
		}
		
		// Check if project location exists
		if (!std::filesystem::exists(m_ProjectLocation)) {
			try {
				std::filesystem::create_directories(m_ProjectLocation);
			}
			catch (const std::exception& e) {
				m_ErrorMessage = "Failed to create project location: " + std::string(e.what());
				return false;
			}
		}
		
		// Check if project already exists
		std::filesystem::path projectPath = std::filesystem::path(m_ProjectLocation) / m_ProjectName;
		if (std::filesystem::exists(projectPath)) {
			m_ErrorMessage = "A project with this name already exists at this location";
			return false;
		}
		
		// Validate window dimensions
		if (m_WindowWidth < 640 || m_WindowHeight < 480) {
			m_ErrorMessage = "Window dimensions must be at least 640x480";
			return false;
		}
		
		return true;
	}
}
