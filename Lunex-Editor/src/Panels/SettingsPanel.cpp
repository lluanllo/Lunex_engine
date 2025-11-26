#include "SettingsPanel.h"
#include "Renderer/Renderer3D.h"
#include "Utils/PlatformUtils.h"
#include "Log/Log.h"
#include "imgui.h"
#include <filesystem>

namespace Lunex {
	void SettingsPanel::OnImGuiRender() {
		ImGui::Begin("Settings");
		
		// ========== RENDERING SETTINGS ==========
		if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Show physics colliders", &m_ShowPhysicsColliders);
		}
		
		// ========== SKYBOX & LIGHTING SETTINGS ==========
		if (ImGui::CollapsingHeader("Skybox & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Enable Skybox", &m_SkyboxSettings.Enabled);
			
			if (m_SkyboxSettings.Enabled) {
				ImGui::Separator();
				
				// ========== SKYBOX MODE SELECTION ==========
				ImGui::SeparatorText("Skybox Type");
				
				const char* skyboxModes[] = { "Procedural (Physical Sky)", "HDRI / Cubemap" };
				int currentMode = m_SkyboxSettings.UseCubemap ? 1 : 0;
				if (ImGui::Combo("##SkyboxMode", &currentMode, skyboxModes, 2)) {
					m_SkyboxSettings.UseCubemap = (currentMode == 1);
					// Clear HDR when switching to procedural
					if (!m_SkyboxSettings.UseCubemap) {
						Renderer3D::GetSkyboxRenderer().UnloadCubemap();
						m_SkyboxSettings.HDRPath = "";
					}
				}
				
				ImGui::Spacing();
				ImGui::Separator();
				
				// ========== MODE-SPECIFIC SETTINGS ==========
				if (m_SkyboxSettings.UseCubemap) {
					// ============ HDRI / CUBEMAP MODE ============
					ImGui::SeparatorText("HDRI Environment");
					
					// Drag & Drop zone
					ImGui::Text("Drop .hdr file here or click to browse:");
					ImGui::BeginChild("HDRDropZone", ImVec2(0, 80), true, ImGuiWindowFlags_NoScrollbar);
					
					ImVec2 dropSize = ImGui::GetContentRegionAvail();
					ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					
					// Draw drop zone background
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					ImU32 dropColor = ImGui::IsWindowHovered() ? IM_COL32(60, 120, 200, 100) : IM_COL32(40, 40, 60, 100);
					drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + dropSize.x, cursorPos.y + dropSize.y), dropColor, 4.0f);
					drawList->AddRect(cursorPos, ImVec2(cursorPos.x + dropSize.x, cursorPos.y + dropSize.y), IM_COL32(100, 150, 255, 200), 4.0f, 0, 2.0f);
					
					// Display current HDR or prompt
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
					if (!m_SkyboxSettings.HDRPath.empty()) {
						std::filesystem::path hdrPath(m_SkyboxSettings.HDRPath);
						std::string filename = hdrPath.filename().string();
						ImGui::TextWrapped("? %s", filename.c_str());
						ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "HDR Loaded Successfully");
					}
					else {
						ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No HDRI loaded");
						ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Drop .hdr file or click Browse button");
					}
					
					// Handle drag & drop
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
							const wchar_t* path = (const wchar_t*)payload->Data;
							std::filesystem::path droppedPath = std::filesystem::path(path);
							
							if (droppedPath.extension() == ".hdr") {
								Renderer3D::GetSkyboxRenderer().LoadHDR(droppedPath.string());
								m_SkyboxSettings.HDRPath = droppedPath.string();
							}
							else {
								LNX_LOG_WARN("Only .hdr files are supported for HDRI");
							}
						}
						ImGui::EndDragDropTarget();
					}
					
					// Make clickable
					if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
						std::string filepath = FileDialogs::OpenFile("HDR Image (*.hdr)\0*.hdr\0");
						if (!filepath.empty()) {
							Renderer3D::GetSkyboxRenderer().LoadHDR(filepath);
							m_SkyboxSettings.HDRPath = filepath;
						}
					}
					
					ImGui::EndChild();
					
					// Buttons
					if (ImGui::Button("Browse HDR...")) {
						std::string filepath = FileDialogs::OpenFile("HDR Image (*.hdr)\0*.hdr\0");
						if (!filepath.empty()) {
							Renderer3D::GetSkyboxRenderer().LoadHDR(filepath);
							m_SkyboxSettings.HDRPath = filepath;
						}
					}
					
					ImGui::SameLine();
					if (ImGui::Button("Load Cubemap (6 faces)")) {
						Renderer3D::GetSkyboxRenderer().LoadDefaultCubemap();
					}
					
					ImGui::SameLine();
					if (ImGui::Button("Clear") && !m_SkyboxSettings.HDRPath.empty()) {
						Renderer3D::GetSkyboxRenderer().UnloadCubemap();
						m_SkyboxSettings.HDRPath = "";
					}
					
					ImGui::Spacing();
					ImGui::Separator();
					
					// HDRI Controls
					ImGui::SeparatorText("HDRI Controls");
					
					ImGui::SliderFloat("Exposure", &m_SkyboxSettings.HDRExposure, 0.1f, 10.0f, "%.2f");
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Adjust brightness of the HDRI");
					
					ImGui::SliderAngle("Rotation", &m_SkyboxSettings.SkyboxRotation, -180.0f, 180.0f);
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Rotate environment to align lighting");
					
					ImGui::Spacing();
					ImGui::TextDisabled("Tip: Use HDRI from Poly Haven (polyhaven.com) for best results");
				}
				else {
					// ============ PROCEDURAL SKY MODE ============
					ImGui::SeparatorText("Procedural Sky (Physical)");
					
					// Time of Day with visual feedback
					ImGui::Text("Time of Day");
					ImGui::SliderFloat("##Hour", &m_SkyboxSettings.TimeOfDay, 0.0f, 24.0f, "%.1f h");
					
					// Visual time indicator
					const char* timeOfDayStr = "Midnight";
					if (m_SkyboxSettings.TimeOfDay >= 5.0f && m_SkyboxSettings.TimeOfDay < 7.0f) timeOfDayStr = "Dawn";
					else if (m_SkyboxSettings.TimeOfDay >= 7.0f && m_SkyboxSettings.TimeOfDay < 17.0f) timeOfDayStr = "Day";
					else if (m_SkyboxSettings.TimeOfDay >= 17.0f && m_SkyboxSettings.TimeOfDay < 19.0f) timeOfDayStr = "Dusk";
					else if (m_SkyboxSettings.TimeOfDay >= 19.0f || m_SkyboxSettings.TimeOfDay < 5.0f) timeOfDayStr = "Night";
					ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Current: %s", timeOfDayStr);
					
					// Time presets
					if (ImGui::Button("Dawn (6h)"))    m_SkyboxSettings.TimeOfDay = 6.0f;
					ImGui::SameLine();
					if (ImGui::Button("Noon (12h)"))    m_SkyboxSettings.TimeOfDay = 12.0f;
					ImGui::SameLine();
					if (ImGui::Button("Dusk (18h)"))    m_SkyboxSettings.TimeOfDay = 18.0f;
					ImGui::SameLine();
					if (ImGui::Button("Midnight (0h)")) m_SkyboxSettings.TimeOfDay = 0.0f;
					
					ImGui::Spacing();
					ImGui::Separator();
					
					// Atmosphere
					ImGui::SeparatorText("Atmosphere");
					
					ImGui::SliderFloat("Sun Intensity", &m_SkyboxSettings.SunIntensity, 0.1f, 5.0f, "%.2f");
					ImGui::SliderFloat("Turbidity", &m_SkyboxSettings.Turbidity, 1.0f, 10.0f, "%.1f");
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "1 = Clear sky, 10 = Very hazy");
					
					// Advanced
					if (ImGui::TreeNode("Advanced Scattering")) {
						ImGui::SliderFloat("Rayleigh", &m_SkyboxSettings.RayleighCoefficient, 0.0f, 3.0f, "%.2f");
						ImGui::SliderFloat("Mie", &m_SkyboxSettings.MieCoefficient, 0.0f, 3.0f, "%.2f");
						ImGui::SliderFloat("Atmosphere Thickness", &m_SkyboxSettings.AtmosphereThickness, 0.0f, 3.0f, "%.2f");
						ImGui::TreePop();
					}
					
					ImGui::Spacing();
					ImGui::Separator();
					
					// Ground
					ImGui::SeparatorText("Ground");
					ImGui::ColorEdit3("Ground Color", &m_SkyboxSettings.GroundColor.x);
					ImGui::SliderFloat("Ground Emission", &m_SkyboxSettings.GroundEmission, 0.0f, 1.0f, "%.2f");
				}
				
				ImGui::Separator();
				
				// ========== COMMON SETTINGS ==========
				ImGui::SeparatorText("Global Settings");
				
				ImGui::SliderFloat("Ambient Intensity", &m_SkyboxSettings.AmbientIntensity, 0.0f, 2.0f, "%.2f");
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Scene ambient light from skybox");
				
				if (!m_SkyboxSettings.UseCubemap) {
					ImGui::SliderFloat("Exposure", &m_SkyboxSettings.Exposure, 0.1f, 5.0f, "%.2f");
				}
				
				ImGui::Separator();
				
				// ========== INFO DISPLAY ==========
				ImGui::SeparatorText("Skybox Info");
				
				auto& skybox = Renderer3D::GetSkyboxRenderer();
				
				if (m_SkyboxSettings.UseCubemap && !m_SkyboxSettings.HDRPath.empty()) {
					std::filesystem::path hdrPath(m_SkyboxSettings.HDRPath);
					ImGui::Text("Type: HDRI");
					ImGui::Text("File: %s", hdrPath.filename().string().c_str());
				}
				else if (m_SkyboxSettings.UseCubemap) {
					ImGui::Text("Type: Cubemap (Traditional)");
				}
				else {
					ImGui::Text("Type: Procedural Physical Sky");
					glm::vec3 sunDir = skybox.GetSunDirection();
					ImGui::Text("Sun Direction: (%.2f, %.2f, %.2f)", sunDir.x, sunDir.y, sunDir.z);
					ImGui::Text("Is Night: %s", skybox.IsNightTime() ? "Yes" : "No");
				}
				
				glm::vec3 ambient = skybox.GetAmbientColor();
				ImGui::ColorEdit3("Current Ambient", &ambient.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
			}
			
			// Apply settings to renderer
			Renderer3D::GetSkyboxRenderer().SetSettings(m_SkyboxSettings);
		}
		
		ImGui::End();
	}
}
