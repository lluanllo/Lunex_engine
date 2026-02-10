/**
 * @file SettingsPanel.cpp
 * @brief Settings Panel Implementation using Lunex UI Framework
 */

#include "stpch.h"
#include "SettingsPanel.h"
#include "ContentBrowserPanel.h"

#include "Renderer/SkyboxRenderer.h"
#include "Scene/Lighting/LightSystem.h"
#include "Log/Log.h"

#include <filesystem>
#include <algorithm>

namespace Lunex {

	void SettingsPanel::OnImGuiRender() {
		using namespace UI;
		
		if (!BeginPanel("Settings")) {
			EndPanel();
			return;
		}
		
		DrawEnvironmentSection();
		
		Separator();
		
		DrawPhysics2DSection();
		
		DrawPhysics3DSection();
		
		EndPanel();
	}
	
	void SettingsPanel::DrawEnvironmentSection() {
		using namespace UI;
		
		if (BeginSection("Environment", true)) {
			// Enable/Disable skybox
			bool skyboxEnabled = SkyboxRenderer::IsEnabled();
			if (PropertyCheckbox("Enable Skybox", skyboxEnabled)) {
				SkyboxRenderer::SetEnabled(skyboxEnabled);
			}
			
			Separator();
			
			// HDRI Path with drag-drop support
			Text("HDRI Environment");
			
			bool hasHDRI = SkyboxRenderer::HasEnvironmentLoaded();
			if (hasHDRI) {
				TextColored(Colors::Success(), "Loaded: %s", 
					m_HDRIPath.empty() ? "(unknown)" : std::filesystem::path(m_HDRIPath).filename().string().c_str());
			} else {
				TextColored(Colors::Warning(), "No HDRI loaded (using background color)");
			}
			
			// Drop target for HDRI files - Manual implementation
			{
				ScopedColor colors({
					{ImGuiCol_Button, Colors::BgMedium()},
					{ImGuiCol_ButtonHovered, Colors::BgHover()},
					{ImGuiCol_Border, Colors::Primary()}
				});
				ScopedStyle borderSize(ImGuiStyleVar_FrameBorderSize, 1.5f);
				
				ImGui::Button("Drop HDRI Here (.hdr, .exr)", ImVec2(-1, 40));
				
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
						std::filesystem::path hdriPath(data->FilePath);
						
						std::string ext = hdriPath.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						
						if (ext == ".hdr" || ext == ".exr") {
							m_HDRIPath = hdriPath.string();
							if (SkyboxRenderer::LoadHDRI(m_HDRIPath)) {
								LNX_LOG_INFO("Loaded HDRI: {0}", m_HDRIPath);
							} else {
								LNX_LOG_ERROR("Failed to load HDRI: {0}", m_HDRIPath);
							}
						} else {
							LNX_LOG_WARN("Invalid file type for HDRI. Expected .hdr or .exr, got: {0}", ext);
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
			
			// Clear HDRI button
			if (hasHDRI) {
				if (Button("Clear HDRI", ButtonVariant::Danger)) {
					SkyboxRenderer::LoadHDRI("");
					m_HDRIPath.clear();
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
			}
			
			Separator();
			
			// ========================================
			// SUN LIGHT SYNCHRONIZATION
			// ========================================
			Text("Sun Light Synchronization");
			AddSpacing(SpacingValues::XS);
			
			bool hasSunLight = LightSystem::Get().HasSunLight();
			bool syncWithSun = SkyboxRenderer::IsSyncWithSunLight();
			
			if (hasSunLight) {
				TextColored(Colors::Success(), "Sun Light: Active");
				
				// Show sun position info
				float elevation = SkyboxRenderer::GetSunElevation();
				float azimuth = SkyboxRenderer::GetSunAzimuth();
				float skyboxRotation = SkyboxRenderer::GetCalculatedSkyboxRotation();
				
				Text("  Elevation: %.1f deg", elevation);
				Text("  Azimuth: %.1f deg", azimuth);
				
				if (syncWithSun) {
					Text("  Skybox Rotation: %.1f deg (synced)", skyboxRotation);
				}
				
				AddSpacing(SpacingValues::XS);
				
				TextWrapped("Sync is controlled by the Directional Light's 'Link to Skybox' setting.", TextVariant::Muted);
				
			} else {
				TextColored(Colors::Warning(), "No Sun Light in scene");
				TextWrapped("Add a Directional Light and enable 'Is Sun Light' to sync skybox rotation.", TextVariant::Muted);
			}
			
			Separator();
			
			// Environment settings (only if HDRI is loaded)
			if (hasHDRI) {
				// Intensity
				float intensity = SkyboxRenderer::GetIntensity();
				if (PropertyFloat("Intensity", intensity, 0.01f, 0.0f, 10.0f)) {
					SkyboxRenderer::SetIntensity(intensity);
				}
				
				// Rotation (manual or synced)
				if (syncWithSun) {
					// Show calculated rotation (read-only)
					float rotation = SkyboxRenderer::GetCalculatedSkyboxRotation();
					BeginDisabled(true);
					PropertyFloat("Rotation (Synced)", rotation, 1.0f, -180.0f, 180.0f);
					EndDisabled();
					if (IsItemHovered()) {
						SetTooltip("Rotation is controlled by the Sun Light.\nDisable 'Link to Skybox' in the light component to use manual rotation.");
					}
				} else {
					// Manual rotation
					float rotation = SkyboxRenderer::GetRotation();
					if (PropertyFloat("Rotation", rotation, 1.0f, -180.0f, 180.0f)) {
						SkyboxRenderer::SetRotation(rotation);
					}
				}
				
				// Blur
				float blur = SkyboxRenderer::GetBlur();
				if (PropertySlider("Blur", blur, 0.0f, 1.0f)) {
					SkyboxRenderer::SetBlur(blur);
				}
				
				// Tint
				glm::vec3 tint = SkyboxRenderer::GetTint();
				if (PropertyColor("Tint", tint)) {
					SkyboxRenderer::SetTint(tint);
				}
			} else {
				// Background color (when no HDRI) - applies to clear color
				glm::vec3 bgColor = SkyboxRenderer::GetBackgroundColor();
				
				// Use manual column layout for color
				BeginColumns(2, false);
				SetColumnWidth(0, SpacingValues::PropertyLabelWidth);
				Label("Background Color");
				NextColumn();
				
				ImGui::SetNextItemWidth(-1);
				if (ImGui::ColorEdit3("##BackgroundColor", glm::value_ptr(bgColor), ImGuiColorEditFlags_NoLabel)) {
					SkyboxRenderer::SetBackgroundColor(bgColor);
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
				
				EndColumns();
			}
			
			EndSection();
		}
	}
	
	void SettingsPanel::DrawPhysics2DSection() {
		using namespace UI;
		
		if (BeginSection("Physics 2D", false)) {
			if (PropertyCheckbox("Show 2D colliders", m_ShowPhysicsColliders, "Display Box2D colliders in red")) {
				// Value updated by reference
			}
			
			EndSection();
		}
	}
	
	void SettingsPanel::DrawPhysics3DSection() {
		using namespace UI;
		
		if (BeginSection("Physics 3D", false)) {
			if (PropertyCheckbox("Show 3D colliders", m_ShowPhysics3DColliders, "Display Bullet3D colliders in green")) {
				// Value updated by reference
			}
			
			EndSection();
		}
	}
}
