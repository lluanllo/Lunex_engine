#include "ToolbarPanel.h"
#include "imgui.h"
#include "Log/Log.h"

namespace Lunex {
	void ToolbarPanel::OnImGuiRender(SceneState sceneState, bool toolbarEnabled,
									 const glm::vec2& viewportBounds, const glm::vec2& viewportSize) {
		
		// No crear ventana si el viewport es muy pequeño
		if (viewportSize.x < 100 || viewportSize.y < 100)
			return;
		
		// Tamaño de los botones
		float buttonSize = 32.0f;
		float spacing = 8.0f;
		float padding = 32.0f;
		
		// Siempre mostrar 2 botones (play/stop y simulate)
		int buttonCount = 2;
		float totalWidth = (buttonSize * buttonCount) + (spacing * (buttonCount - 1)) + (padding * 2);
		float totalHeight = buttonSize + (padding * 2);
		
		// Calcular posición centrada en la parte superior del viewport
		float toolbarX = viewportBounds.x + (viewportSize.x * 0.5f) - (totalWidth * 0.5f);
		float toolbarY = viewportBounds.y - 20.0f;
		
		ImGui::SetNextWindowPos(ImVec2(toolbarX, toolbarY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
		
		// Estilo del contenedor (completamente transparente, sin marco)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		
		// Contenedor completamente transparente
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		
		// Flags para que la toolbar esté siempre en primer plano
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | 
									   ImGuiWindowFlags_NoMove | 
									   ImGuiWindowFlags_NoScrollbar | 
									   ImGuiWindowFlags_NoScrollWithMouse |
									   ImGuiWindowFlags_NoSavedSettings |
									   ImGuiWindowFlags_NoFocusOnAppearing |
									   ImGuiWindowFlags_NoDocking;
		
		ImGui::Begin("##FloatingToolbar", nullptr, windowFlags);
		
		ImVec4 tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		if (!toolbarEnabled)
			tintColor.w = 0.4f;
		
		// Estilo de los botones - translúcidos con efecto sutil
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, 0.9f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		
		// ========== PLAY/STOP BUTTON ==========
		{
			bool isPlaying = (sceneState == SceneState::Play);
			Ref<Texture2D> icon = isPlaying ? m_IconStop : m_IconPlay;
			const char* label = isPlaying ? "■" : "▶";
			
			bool clicked = false;
			
			// SIEMPRE renderizar botón, con icono o con texto
			if (icon) {
				clicked = ImGui::ImageButton("##PlayButton", 
					(ImTextureID)(intptr_t)icon->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				// Fallback: botón con texto
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button(label, ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked && toolbarEnabled) {
				if (sceneState == SceneState::Edit || sceneState == SceneState::Simulate) {
					LNX_LOG_INFO("Play button clicked");
					if (m_OnPlayCallback)
						m_OnPlayCallback();
				}
				else if (sceneState == SceneState::Play) {
					LNX_LOG_INFO("Stop button clicked");
					if (m_OnStopCallback)
						m_OnStopCallback();
				}
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text(isPlaying ? "Stop (Ctrl+P)" : "Play (Ctrl+P)");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
		}
		
		ImGui::SameLine();
		
		// ========== SIMULATE BUTTON ==========
		{
			bool isSimulating = (sceneState == SceneState::Simulate);
			Ref<Texture2D> icon = isSimulating ? m_IconStop : m_IconSimulate;
			const char* label = isSimulating ? "■" : "🔁";
			
			bool clicked = false;
			
			// SIEMPRE renderizar botón, con icono o con texto
			if (icon) {
				clicked = ImGui::ImageButton("##SimulateButton",
					(ImTextureID)(intptr_t)icon->GetRendererID(),
					ImVec2(buttonSize, buttonSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 0, 0, 0), tintColor);
			} else {
				// Fallback: botón con texto
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				clicked = ImGui::Button(label, ImVec2(buttonSize, buttonSize));
				ImGui::PopStyleVar();
			}
			
			if (clicked && toolbarEnabled) {
				if (sceneState == SceneState::Edit || sceneState == SceneState::Play) {
					LNX_LOG_INFO("Simulate button clicked");
					if (m_OnSimulateCallback)
						m_OnSimulateCallback();
				}
				else if (sceneState == SceneState::Simulate) {
					LNX_LOG_INFO("Stop simulate button clicked");
					if (m_OnStopCallback)
						m_OnStopCallback();
				}
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text(isSimulating ? "Stop Simulation" : "Simulate Physics");
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
				ImGui::PopStyleVar();
			}
		}
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		
		ImGui::End();
		
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}
}
