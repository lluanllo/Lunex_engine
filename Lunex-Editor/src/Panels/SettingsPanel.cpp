#include "SettingsPanel.h"
#include "Renderer/Renderer3D.h"
#include "imgui.h"

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
				
				// ========== SKYBOX MODE ==========
				ImGui::Text("Skybox Mode");
				const char* skyboxModes[] = { "Procedural (Physical)", "Cubemap" };
				int currentMode = m_SkyboxSettings.UseCubemap ? 1 : 0;
				if (ImGui::Combo("Mode", &currentMode, skyboxModes, 2)) {
					m_SkyboxSettings.UseCubemap = (currentMode == 1);
				}
				
				ImGui::Separator();
				
				// ========== MODE-SPECIFIC SETTINGS ==========
				if (m_SkyboxSettings.UseCubemap) {
					// Cubemap settings
					ImGui::Text("Cubemap Settings");
					
					if (ImGui::Button("Load Default Cubemap")) {
						Renderer3D::GetSkyboxRenderer().LoadDefaultCubemap();
					}
					
					ImGui::TextWrapped("Note: Place cubemap faces in assets/textures/skybox/");
					ImGui::TextWrapped("  right.jpg, left.jpg, top.jpg, bottom.jpg, front.jpg, back.jpg");
				}
				else {
					// ========== PROCEDURAL SETTINGS ==========
					ImGui::SeparatorText("Time & Sun");
					
					// Time of Day with visual feedback
					ImGui::Text("Time of Day");
					ImGui::SliderFloat("Hour", &m_SkyboxSettings.TimeOfDay, 0.0f, 24.0f, "%.1f h");
					
					// Time presets
					ImGui::Text("Presets:");
					if (ImGui::Button("Dawn"))    m_SkyboxSettings.TimeOfDay = 6.0f;
					ImGui::SameLine();
					if (ImGui::Button("Noon"))    m_SkyboxSettings.TimeOfDay = 12.0f;
					ImGui::SameLine();
					if (ImGui::Button("Dusk"))    m_SkyboxSettings.TimeOfDay = 18.0f;
					ImGui::SameLine();
					if (ImGui::Button("Midnight")) m_SkyboxSettings.TimeOfDay = 0.0f;
					
					ImGui::SliderFloat("Sun Intensity", &m_SkyboxSettings.SunIntensity, 0.1f, 5.0f, "%.2f");
					
					ImGui::Separator();
					
					// ========== ATMOSPHERIC PARAMETERS ==========
					ImGui::SeparatorText("Atmosphere");
					
					ImGui::SliderFloat("Turbidity", &m_SkyboxSettings.Turbidity, 1.0f, 10.0f, "%.1f");
					ImGui::TextWrapped("Atmospheric haziness (1=clear, 10=very hazy)");
					
					ImGui::Spacing();
					
					if (ImGui::TreeNode("Advanced Scattering")) {
						ImGui::SliderFloat("Rayleigh Coefficient", &m_SkyboxSettings.RayleighCoefficient, 0.0f, 3.0f, "%.2f");
						ImGui::TextWrapped("Controls sky blue color intensity");
						
						ImGui::SliderFloat("Mie Coefficient", &m_SkyboxSettings.MieCoefficient, 0.0f, 3.0f, "%.2f");
						ImGui::TextWrapped("Controls atmospheric haze/fog");
						
						ImGui::SliderFloat("Mie Directional G", &m_SkyboxSettings.MieDirectionalG, -0.99f, 0.99f, "%.3f");
						ImGui::TextWrapped("Forward scattering anisotropy");
						
						ImGui::SliderFloat("Atmosphere Thickness", &m_SkyboxSettings.AtmosphereThickness, 0.0f, 3.0f, "%.2f");
						
						ImGui::TreePop();
					}
					
					ImGui::Separator();
					
					// ========== GROUND ==========
					ImGui::SeparatorText("Ground");
					
					ImGui::ColorEdit3("Ground Color", &m_SkyboxSettings.GroundColor.x);
					ImGui::SliderFloat("Ground Emission", &m_SkyboxSettings.GroundEmission, 0.0f, 1.0f, "%.2f");
					ImGui::TextWrapped("Ground reflectivity/emission");
				}
				
				ImGui::Separator();
				
				// ========== POST-PROCESSING (common for both modes) ==========
				ImGui::SeparatorText("Post-Processing");
				
				ImGui::SliderFloat("Exposure", &m_SkyboxSettings.Exposure, 0.1f, 5.0f, "%.2f");
				ImGui::SliderFloat("Sky Tint", &m_SkyboxSettings.SkyTint, 0.0f, 2.0f, "%.2f");
				
				ImGui::Separator();
				
				// ========== GLOBAL LIGHTING ==========
				ImGui::SeparatorText("Global Lighting");
				
				ImGui::SliderFloat("Ambient Intensity", &m_SkyboxSettings.AmbientIntensity, 0.0f, 1.0f, "%.3f");
				ImGui::TextWrapped("Controls how much the skybox affects scene lighting");
				
				ImGui::Separator();
				
				// ========== INFO DISPLAY ==========
				ImGui::SeparatorText("Current State");
				
				auto& skybox = Renderer3D::GetSkyboxRenderer();
				glm::vec3 sunDir = skybox.GetSunDirection();
				glm::vec3 sunColor = skybox.GetSunColor();
				glm::vec3 ambient = skybox.GetAmbientColor();
				
				ImGui::Text("Sun Direction: (%.2f, %.2f, %.2f)", sunDir.x, sunDir.y, sunDir.z);
				ImGui::ColorEdit3("Sun Color", &sunColor.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
				ImGui::ColorEdit3("Ambient Color", &ambient.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
				ImGui::Text("Is Night: %s", skybox.IsNightTime() ? "Yes" : "No");
				
				ImGui::Spacing();
				ImGui::TextDisabled("Tip: This is a physically-based atmospheric scattering model");
				ImGui::TextDisabled("similar to Blender's Sky Texture node.");
			}
			
			// Apply settings to renderer
			Renderer3D::GetSkyboxRenderer().SetSettings(m_SkyboxSettings);
		}
		
		ImGui::End();
	}
}
