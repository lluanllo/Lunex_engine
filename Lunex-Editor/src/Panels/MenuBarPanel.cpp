/**
 * @file MenuBarPanel.cpp
 * @brief Menu Bar Panel - Main application menu bar
 * 
 * Features:
 * - File menu (Project & Scene operations)
 * - Edit menu (Undo/Redo, Clipboard)
 * - View menu (Panel visibility)
 * - Preferences menu (Settings)
 * - Help menu
 * - Logo display
 * - Project/Scene name display
 */

#include "stpch.h"
#include "MenuBarPanel.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	// ============================================================================
	// MENU BAR STYLE CONSTANTS
	// ============================================================================
	
	namespace MenuBarStyle {
		constexpr float LogoPadding = 4.0f;
		constexpr float LogoSpacing = 10.0f;
		constexpr float FramePaddingX = 20.0f;
		constexpr float FramePaddingY = 20.0f;
		
		inline UI::Color SceneNameColor()   { return UI::Color(0.80f, 0.80f, 0.80f, 1.0f); }
		inline UI::Color ProjectNameColor() { return UI::Color(0.60f, 0.60f, 0.60f, 1.0f); }
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void MenuBarPanel::OnImGuiRender() {
		using namespace UI;
		
		// Increase menu bar height
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(MenuBarStyle::FramePaddingX, MenuBarStyle::FramePaddingY));

		if (BeginMenuBar()) {
			// Logo
			RenderLogo();
			
			// File Menu
			RenderFileMenu();
			
			// Edit Menu
			RenderEditMenu();
			
			// View Menu
			RenderViewMenu();
			
			// Preferences Menu
			RenderPreferencesMenu();
			
			// Help Menu
			RenderHelpMenu();
			
			// Scene & Project names (right side)
			RenderStatusInfo();

			EndMenuBar();
		}

		ImGui::PopStyleVar();
	}

	// ============================================================================
	// LOGO
	// ============================================================================

	void MenuBarPanel::RenderLogo() {
		if (!m_LogoTexture) {
			m_LogoTexture = Texture2D::Create("Resources/Icons/LunexLogo/LunexLogo.png");
		}

		if (m_LogoTexture && m_LogoTexture->IsLoaded()) {
			float menuBarHeight = ImGui::GetFrameHeight();
			float logoSize = menuBarHeight - MenuBarStyle::LogoPadding;

			// Center vertically
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

			ImGui::Image(
				(void*)(intptr_t)m_LogoTexture->GetRendererID(),
				ImVec2(logoSize, logoSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
			
			UI::SameLine();
			ImGui::Dummy(ImVec2(MenuBarStyle::LogoSpacing, 0.0f));
			UI::SameLine();
		}
	}

	// ============================================================================
	// FILE MENU
	// ============================================================================

	void MenuBarPanel::RenderFileMenu() {
		using namespace UI;
		
		if (BeginMenu("File")) {
			// Project section
			SeparatorText("Project");

			if (MenuItem("New Project", "Ctrl+Shift+N")) {
				if (m_OnNewProject) m_OnNewProject();
			}

			if (MenuItem("Open Project...", "Ctrl+Shift+O")) {
				if (m_OnOpenProject) m_OnOpenProject();
			}

			if (MenuItem("Save Project", "Ctrl+Shift+S")) {
				if (m_OnSaveProject) m_OnSaveProject();
			}

			if (MenuItem("Save Project As...")) {
				if (m_OnSaveProjectAs) m_OnSaveProjectAs();
			}

			Separator();

			// Scene section
			SeparatorText("Scene");

			if (MenuItem("New Scene", "Ctrl+N")) {
				if (m_OnNewScene) m_OnNewScene();
			}

			if (MenuItem("Open Scene...", "Ctrl+O")) {
				if (m_OnOpenScene) m_OnOpenScene();
			}

			if (MenuItem("Save Scene", "Ctrl+S")) {
				if (m_OnSaveScene) m_OnSaveScene();
			}

			if (MenuItem("Save Scene As...", "Ctrl+Alt+S")) {
				if (m_OnSaveSceneAs) m_OnSaveSceneAs();
			}

			Separator();

			if (MenuItem("Exit")) {
				if (m_OnExit) m_OnExit();
			}

			EndMenu();
		}
	}

	// ============================================================================
	// EDIT MENU
	// ============================================================================

	void MenuBarPanel::RenderEditMenu() {
		using namespace UI;
		
		if (BeginMenu("Edit")) {
			MenuItem("Undo", "Ctrl+Z", false, false);
			MenuItem("Redo", "Ctrl+Y", false, false);
			Separator();
			MenuItem("Cut", "Ctrl+X", false, false);
			MenuItem("Copy", "Ctrl+C", false, false);
			MenuItem("Paste", "Ctrl+V", false, false);
			EndMenu();
		}
	}

	// ============================================================================
	// VIEW MENU
	// ============================================================================

	void MenuBarPanel::RenderViewMenu() {
		using namespace UI;
		
		if (BeginMenu("View")) {
			MenuItem("Scene Hierarchy", nullptr, false, false);
			MenuItem("Properties", nullptr, false, false);
			MenuItem("Content Browser", nullptr, false, false);
			MenuItem("Console", nullptr, false, false);
			MenuItem("Stats", nullptr, false, false);
			EndMenu();
		}
	}

	// ============================================================================
	// PREFERENCES MENU
	// ============================================================================

	void MenuBarPanel::RenderPreferencesMenu() {
		using namespace UI;
		
		if (BeginMenu("Preferences")) {
			if (MenuItem("Input Settings", "Ctrl+K")) {
				if (m_OnOpenInputSettings) m_OnOpenInputSettings();
			}
			
			if (MenuItem("Outline & Colliders")) {
				if (m_OnOpenOutlinePreferences) m_OnOpenOutlinePreferences();
			}
			
			if (MenuItem("JobSystem Monitor")) {
				if (m_OnOpenJobSystemPanel) m_OnOpenJobSystemPanel();
			}
			
			Separator();
			MenuItem("Editor Settings", nullptr, false, false);
			MenuItem("Theme", nullptr, false, false);
			EndMenu();
		}
	}

	// ============================================================================
	// HELP MENU
	// ============================================================================

	void MenuBarPanel::RenderHelpMenu() {
		using namespace UI;
		
		if (BeginMenu("Help")) {
			MenuItem("Documentation");
			MenuItem("About Lunex Editor");
			EndMenu();
		}
	}

	// ============================================================================
	// STATUS INFO (Scene & Project names)
	// ============================================================================

	void MenuBarPanel::RenderStatusInfo() {
		using namespace UI;
		
		float availWidth = ImGui::GetContentRegionAvail().x;

		// Calculate text widths
		float sceneTextWidth = ImGui::CalcTextSize(m_SceneName.c_str()).x;
		float versionTextWidth = ImGui::CalcTextSize(m_ProjectName.c_str()).x;

		// Center position for scene name
		float centerPos = (availWidth - sceneTextWidth) * 0.5f;

		// Check if there's enough space
		if (centerPos > 10.0f && centerPos + sceneTextWidth < availWidth - versionTextWidth - 20.0f) {
			// Scene name (centered)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerPos);
			
			{
				ScopedColor sceneColor(ImGuiCol_Text, MenuBarStyle::SceneNameColor());
				ImGui::Text("%s", m_SceneName.c_str());
			}

			// Project name (right-aligned)
			SameLine();
			float rightPos = availWidth - versionTextWidth - 10.0f;
			if (rightPos > ImGui::GetCursorPosX()) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightPos - ImGui::GetCursorPosX());
				
				ScopedColor projectColor(ImGuiCol_Text, MenuBarStyle::ProjectNameColor());
				ImGui::Text("%s", m_ProjectName.c_str());
			}
		}
	}

} // namespace Lunex