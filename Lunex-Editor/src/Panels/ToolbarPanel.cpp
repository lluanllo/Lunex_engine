#include "stpch.h"
#include "ToolbarPanel.h"
#include <imgui.h>

namespace Lunex {
	ToolbarPanel::ToolbarPanel() {
		m_IconPlay = Texture2D::Create("Resources/Icons/PlayStopButtons/PlayButtonIcon.png");
		m_IconSimulate = Texture2D::Create("Resources/Icons/PlayStopButtons/SimulateButtonIcon.png");
		m_IconStop = Texture2D::Create("Resources/Icons/PlayStopButtons/StopButtonIcon.png");
	}
	
	void ToolbarPanel::OnImGuiRender() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		
		auto& colors = ImGui::GetStyle().Colors;
		const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
		const auto& buttonActive = colors[ImGuiCol_ButtonActive];
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));
		
		ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		
		ImVec4 tintColor = ImVec4(1, 1, 1, 1);
		if (!m_ToolbarEnabled)
			tintColor.w = 0.5f;
		
		float size = ImGui::GetWindowHeight() - 4.0f;
		
		// Play/Stop button
		{
			Ref<Texture2D> icon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) ? m_IconPlay : m_IconStop;
			ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));
			
			if (ImGui::ImageButton("##PlayButton", (ImTextureID)(intptr_t)icon->GetRendererID(),
				ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tintColor) && m_ToolbarEnabled)
			{
				if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) {
					if (m_Callbacks.OnPlay) m_Callbacks.OnPlay();
				}
				else if (m_SceneState == SceneState::Play) {
					if (m_Callbacks.OnStop) m_Callbacks.OnStop();
				}
			}
		}
		
		ImGui::SameLine();
		
		// Simulate/Stop button
		{
			Ref<Texture2D> icon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play) ? m_IconSimulate : m_IconStop;
			
			if (ImGui::ImageButton("##SimulateButton", (ImTextureID)(intptr_t)icon->GetRendererID(),
				ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tintColor) && m_ToolbarEnabled)
			{
				if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play) {
					if (m_Callbacks.OnSimulate) m_Callbacks.OnSimulate();
				}
				else if (m_SceneState == SceneState::Simulate) {
					if (m_Callbacks.OnStop) m_Callbacks.OnStop();
				}
			}
		}
		
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);
		ImGui::End();
	}
}