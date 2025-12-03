#include "stpch.h"
#include "MenuBarPanel.h"
#include <imgui.h>

namespace Lunex {
	void MenuBarPanel::OnImGuiRender() {
		// ✅ FIX: Aumentar altura de la menu bar significativamente
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20.0f, 20.0f)); // Padding horizontal y vertical

		if (ImGui::BeginMenuBar()) {
			// Renderizar logo al inicio de la barra de menú
			if (!m_LogoTexture) {
				m_LogoTexture = Texture2D::Create("Resources/Icons/LunexLogo/LunexLogo.png");
			}

			if (m_LogoTexture && m_LogoTexture->IsLoaded()) {
				float menuBarHeight = ImGui::GetFrameHeight();
				// ✅ FIX: Logo más grande y mejor centrado
				float logoSize = menuBarHeight - 4.0f; // Más grande (era -8.0f)

				// ✅ FIX: Centrar verticalmente el logo
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

				ImGui::Image(
					(void*)(intptr_t)m_LogoTexture->GetRendererID(),
					ImVec2(logoSize, logoSize),
					ImVec2(0, 1), ImVec2(1, 0) // Flip vertical para OpenGL
				);
				ImGui::SameLine();
				// ✅ FIX: Más espacio después del logo
				ImGui::Dummy(ImVec2(10.0f, 0.0f));
				ImGui::SameLine();
			}

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
				if (ImGui::MenuItem("Input Settings", "Ctrl+K")) {
					if (m_OnOpenInputSettings) m_OnOpenInputSettings();
				}
				
				if (ImGui::MenuItem("JobSystem Monitor")) {
					if (m_OnOpenJobSystemPanel) m_OnOpenJobSystemPanel();
				}
				
				ImGui::Separator();
				if (ImGui::MenuItem("Editor Settings", nullptr, false, false)) {
					// TODO: Implement editor settings
				}
				if (ImGui::MenuItem("Theme", nullptr, false, false)) {
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

			// Calculate available space
			float availWidth = ImGui::GetContentRegionAvail().x;

			// Display scene name in the center
			std::string sceneText = m_SceneName;
			float sceneTextWidth = ImGui::CalcTextSize(sceneText.c_str()).x;

			// Display project version on the right
			std::string versionText = m_ProjectName;
			float versionTextWidth = ImGui::CalcTextSize(versionText.c_str()).x;

			// Calculate center position for scene name
			float centerPos = (availWidth - sceneTextWidth) * 0.5f;

			// Make sure scene name doesn't overlap with version
			if (centerPos > 10.0f && centerPos + sceneTextWidth < availWidth - versionTextWidth - 20.0f) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerPos);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
				ImGui::Text("%s", m_SceneName.c_str());
				ImGui::PopStyleColor();

				// Display version on the right
				ImGui::SameLine();
				float rightPos = availWidth - versionTextWidth - 10.0f;
				if (rightPos > ImGui::GetCursorPosX()) {
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightPos - ImGui::GetCursorPosX());
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
					ImGui::Text("%s", m_ProjectName.c_str());
					ImGui::PopStyleColor();
				}
			}

			ImGui::EndMenuBar();
		}

		ImGui::PopStyleVar(); // FramePadding
	}
}