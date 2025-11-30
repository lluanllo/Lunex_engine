#include "stpch.h"
#include "MenuBarPanel.h"
#include <imgui.h>

namespace Lunex {
	void MenuBarPanel::OnImGuiRender() {
		if (ImGui::BeginMenuBar()) {
			// File Menu
			if (ImGui::BeginMenu("File")) {
				// Project section
				ImGui::SeparatorText("Project");
				
				if (ImGui::MenuItem("New Project", "Ctrl+Shift+N")) {
					if (m_OnNewProject) m_OnNewProject();
				}
				
				if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O")) {
					if (m_OnOpenProject) m_OnOpenProject();
				}
				
				if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S")) {
					if (m_OnSaveProject) m_OnSaveProject();
				}
				
				if (ImGui::MenuItem("Save Project As...")) {
					if (m_OnSaveProjectAs) m_OnSaveProjectAs();
				}
				
				ImGui::Separator();
				
				// Scene section
				ImGui::SeparatorText("Scene");
				
				if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
					if (m_OnNewScene) m_OnNewScene();
				}
				
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
					if (m_OnOpenScene) m_OnOpenScene();
				}
				
				if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
					if (m_OnSaveScene) m_OnSaveScene();
				}
				
				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Alt+S")) {
					if (m_OnSaveSceneAs) m_OnSaveSceneAs();
				}
				
				ImGui::Separator();
				
				if (ImGui::MenuItem("Exit")) {
					if (m_OnExit) m_OnExit();
				}
				
				ImGui::EndMenu();
			}
			
			// Edit Menu
			if (ImGui::BeginMenu("Edit")) {
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
				if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "Ctrl+X", false, false)) {}
				if (ImGui::MenuItem("Copy", "Ctrl+C", false, false)) {}
				if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {}
				ImGui::EndMenu();
			}
			
			// View Menu
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Scene Hierarchy", nullptr, nullptr, false);
				ImGui::MenuItem("Properties", nullptr, nullptr, false);
				ImGui::MenuItem("Content Browser", nullptr, nullptr, false);
				ImGui::MenuItem("Console", nullptr, nullptr, false);
				ImGui::MenuItem("Stats", nullptr, nullptr, false);
				ImGui::EndMenu();
			}
			
			// Preferences Menu
			if (ImGui::BeginMenu("Preferences")) {
				if (ImGui::MenuItem("?? Input Settings", "Ctrl+K")) {
					if (m_OnOpenInputSettings) m_OnOpenInputSettings();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("?? Editor Settings", nullptr, false, false)) {
					// TODO: Implement editor settings
				}
				if (ImGui::MenuItem("?? Theme", nullptr, false, false)) {
					// TODO: Implement theme selection
				}
				ImGui::EndMenu();
			}
			
			// Help Menu
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("Documentation")) {}
				if (ImGui::MenuItem("About Lunex Editor")) {}
				ImGui::EndMenu();
			}
			
			// Display project name on the right side
			float availWidth = ImGui::GetContentRegionAvail().x;
			std::string projectText = m_ProjectName;
			float textWidth = ImGui::CalcTextSize(projectText.c_str()).x;
			
			if (availWidth > textWidth + 20.0f) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - textWidth - 10.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("%s", m_ProjectName.c_str());
				ImGui::PopStyleColor();
			}
			
			ImGui::EndMenuBar();
		}
	}
}
