#include "stpch.h"
#include "ProjectCreationDialog.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>
#include <imgui_internal.h>

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
		
		ImGui::OpenPopup("Create New Project");
		
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Appearing);
		
		if (ImGui::BeginPopupModal("Create New Project", &m_IsOpen, ImGuiWindowFlags_NoResize)) {
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));
			
			// Header
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			ImGui::SetWindowFontScale(1.2f);
			ImGui::Text("Create New Lunex Project");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			
			ImGui::Separator();
			ImGui::Spacing();
			
			// Project Name
			ImGui::Text("Project Name");
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##ProjectName", m_ProjectName, sizeof(m_ProjectName));
			
			ImGui::Spacing();
			
			// Project Location
			ImGui::Text("Project Location");
			ImGui::PushItemWidth(-80);
			ImGui::InputText("##ProjectLocation", m_ProjectLocation, sizeof(m_ProjectLocation));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Browse...", ImVec2(70, 0))) {
				BrowseProjectLocation();
			}
			
			// Show full path preview
			std::filesystem::path fullPath = std::filesystem::path(m_ProjectLocation) / m_ProjectName;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::TextWrapped("Project will be created at: %s", fullPath.string().c_str());
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Template Selection
			ImGui::Text("Template");
			ImGui::SetNextItemWidth(-1);
			ImGui::Combo("##Template", &m_SelectedTemplate, m_Templates, IM_ARRAYSIZE(m_Templates));
			
			// Template description
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
			switch (m_SelectedTemplate) {
			case 0:
				ImGui::TextWrapped("Empty project with basic folder structure");
				break;
			case 1:
				ImGui::TextWrapped("3D project with camera, light, and cube");
				break;
			case 2:
				ImGui::TextWrapped("2D project with orthographic camera and sprite");
				break;
			}
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Project Settings
			if (ImGui::CollapsingHeader("Project Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Indent();
				
				ImGui::Text("Window Size");
				ImGui::SetNextItemWidth(150);
				ImGui::InputInt("Width##WindowWidth", &m_WindowWidth);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150);
				ImGui::InputInt("Height##WindowHeight", &m_WindowHeight);
				
				ImGui::Spacing();
				ImGui::Checkbox("VSync", &m_VSync);
				ImGui::SameLine();
				ImGui::Checkbox("Fullscreen", &m_Fullscreen);
				
				ImGui::Unindent();
			}
			
			ImGui::Spacing();
			
			// Error message
			if (!m_ErrorMessage.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
				ImGui::TextWrapped("%s", m_ErrorMessage.c_str());
				ImGui::PopStyleColor();
				ImGui::Spacing();
			}
			
			// Buttons
			ImGui::Separator();
			ImGui::Spacing();
			
			float buttonWidth = 120.0f;
			float spacing = 10.0f;
			float totalWidth = (buttonWidth * 2) + spacing;
			float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
			
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.4f, 0.8f, 1.0f));
			
			if (ImGui::Button("Create", ImVec2(buttonWidth, 35))) {
				if (ValidateInput()) {
					CreateProject();
					Close();
				}
			}
			
			ImGui::PopStyleColor(3);
			
			ImGui::SameLine(0, spacing);
			
			if (ImGui::Button("Cancel", ImVec2(buttonWidth, 35))) {
				Close();
			}
			
			ImGui::PopStyleVar(2);
			ImGui::EndPopup();
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
