#include "SettingsPanel.h"
#include "imgui.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/SSRRenderer.h"
#include "Log/Log.h"
#include "ContentBrowserPanel.h"  // For ContentBrowserPayload

#include <filesystem>

namespace Lunex {
	
	void SettingsPanel::OnImGuiRender() {
		ImGui::Begin("Settings");
		
		// ========================================
		// TAB BAR
		// ========================================
		if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
			
			// World Tab
			if (ImGui::BeginTabItem("World")) {
				RenderWorldTab();
				ImGui::EndTabItem();
			}
			
			// Physics Tab
			if (ImGui::BeginTabItem("Physics")) {
				RenderPhysicsTab();
				ImGui::EndTabItem();
			}
			
			// Render Tab
			if (ImGui::BeginTabItem("Render")) {
				RenderRenderTab();
				ImGui::EndTabItem();
			}
			
			ImGui::EndTabBar();
		}
		
		ImGui::End();
	}
	
	// ========================================
	// WORLD TAB - Environment/Skybox Settings
	// ========================================
	void SettingsPanel::RenderWorldTab() {
		ImGui::Spacing();
		
		// ========================================
		// SKYBOX / ENVIRONMENT
		// ========================================
		if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
			// Enable/Disable skybox
			bool skyboxEnabled = SkyboxRenderer::IsEnabled();
			if (ImGui::Checkbox("Enable Skybox", &skyboxEnabled)) {
				SkyboxRenderer::SetEnabled(skyboxEnabled);
			}
			
			ImGui::Separator();
			
			// HDRI Path with drag-drop support
			ImGui::Text("HDRI Environment");
			
			// Display current path
			bool hasHDRI = SkyboxRenderer::HasEnvironmentLoaded();
			if (hasHDRI) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Loaded: %s", 
					m_HDRIPath.empty() ? "(unknown)" : std::filesystem::path(m_HDRIPath).filename().string().c_str());
			} else {
				ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "No HDRI loaded (using background color)");
			}
			
			// Drop target for HDRI files
			ImGui::Button("Drop HDRI Here", ImVec2(-1, 40));
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
			
			// Clear HDRI button
			if (hasHDRI) {
				if (ImGui::Button("Clear HDRI")) {
					SkyboxRenderer::LoadHDRI("");
					m_HDRIPath.clear();
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
			}
			
			ImGui::Separator();
			
			// Environment settings (only if HDRI is loaded)
			if (hasHDRI) {
				// Intensity
				float intensity = SkyboxRenderer::GetIntensity();
				if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f)) {
					SkyboxRenderer::SetIntensity(intensity);
				}
				
				// Rotation
				float rotation = SkyboxRenderer::GetRotation();
				if (ImGui::DragFloat("Rotation", &rotation, 1.0f, -180.0f, 180.0f, "%.1f deg")) {
					SkyboxRenderer::SetRotation(rotation);
				}
				
				// Blur
				float blur = SkyboxRenderer::GetBlur();
				if (ImGui::SliderFloat("Blur", &blur, 0.0f, 1.0f)) {
					SkyboxRenderer::SetBlur(blur);
				}
				
				// Tint
				glm::vec3 tint = SkyboxRenderer::GetTint();
				if (ImGui::ColorEdit3("Tint", &tint.x)) {
					SkyboxRenderer::SetTint(tint);
				}
			} else {
				// Background color (when no HDRI)
				glm::vec3 bgColor = SkyboxRenderer::GetBackgroundColor();
				if (ImGui::ColorEdit3("Background Color", &bgColor.x)) {
					SkyboxRenderer::SetBackgroundColor(bgColor);
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
			}
		}
	}
	
	// ========================================
	// PHYSICS TAB - Collider Visualization
	// ========================================
	void SettingsPanel::RenderPhysicsTab() {
		ImGui::Spacing();
		
		// ========================================
		// PHYSICS 2D
		// ========================================
		if (ImGui::CollapsingHeader("Physics 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Show 2D Colliders", &m_ShowPhysicsColliders);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Display Box2D colliders in red");
			}
			
			// Future: Add more 2D physics settings
			// ImGui::DragFloat("Gravity Y", ...);
		}
		
		ImGui::Spacing();
		
		// ========================================
		// PHYSICS 3D
		// ========================================
		if (ImGui::CollapsingHeader("Physics 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Show 3D Colliders", &m_ShowPhysics3DColliders);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Display Bullet3D colliders in green");
			}
			
			// Future: Add more 3D physics settings
			// ImGui::DragFloat3("Gravity", ...);
		}
	}
	
	// ========================================
	// RENDER TAB - Post-Processing & Effects
	// ========================================
	void SettingsPanel::RenderRenderTab() {
		ImGui::Spacing();
		
		// ========================================
		// SCREEN SPACE REFLECTIONS (SSR)
		// ========================================
		if (ImGui::CollapsingHeader("Screen Space Reflections", ImGuiTreeNodeFlags_DefaultOpen)) {
			SSRSettings& ssr = SSRRenderer::GetSettings();
			
			// Enable/Disable
			if (ImGui::Checkbox("Enable SSR", &ssr.Enabled)) {
				SSRRenderer::SetEnabled(ssr.Enabled);
			}
			
			if (ssr.Enabled) {
				ImGui::Separator();
				
				// Quality Settings
				ImGui::Text("Quality");
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				
				ImGui::DragFloat("Max Distance", &ssr.MaxDistance, 1.0f, 1.0f, 500.0f, "%.0f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Maximum ray travel distance in world units");
				}
				
				ImGui::DragInt("Max Steps", &ssr.MaxSteps, 1, 16, 512);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Maximum ray march iterations (higher = better quality, slower)");
				}
				
				ImGui::DragFloat("Thickness", &ssr.Thickness, 0.01f, 0.01f, 2.0f, "%.3f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Depth comparison threshold for hit detection (higher = more tolerant)");
				}
				
				ImGui::SliderFloat("Resolution", &ssr.Resolution, 0.25f, 1.0f, "%.2f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Render resolution scale (0.5 = half resolution)");
				}
				
				ImGui::PopStyleColor();
				
				ImGui::Separator();
				
				// Visual Settings
				ImGui::Text("Visual");
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				
				ImGui::SliderFloat("Intensity", &ssr.Intensity, 0.0f, 2.0f, "%.2f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reflection intensity multiplier");
				}
				
				ImGui::SliderFloat("Edge Fade", &ssr.EdgeFade, 0.0f, 0.5f, "%.2f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Fade reflections near screen edges");
				}
				
				ImGui::SliderFloat("Roughness Threshold", &ssr.RoughnessThreshold, 0.0f, 1.0f, "%.2f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Maximum roughness for SSR (higher roughness = no SSR)");
				}
				
				ImGui::PopStyleColor();
				
				ImGui::Separator();
				
				// Environment Fallback
				ImGui::Text("Environment Fallback");
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				
				ImGui::Checkbox("Use Skybox Fallback", &ssr.UseEnvironmentFallback);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("When SSR doesn't find a hit, use the environment/skybox for reflection");
				}
				
				if (ssr.UseEnvironmentFallback) {
					ImGui::SliderFloat("Fallback Intensity", &ssr.EnvironmentIntensity, 0.0f, 2.0f, "%.2f");
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Intensity of environment reflections when SSR misses");
					}
				}
				
				ImGui::PopStyleColor();
				
				ImGui::Separator();
				
				// Debug
				ImGui::Checkbox("Debug Mode", &ssr.DebugMode);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Show only SSR contribution (no scene color)");
				}
				
				// Presets
				ImGui::Separator();
				ImGui::Text("Presets");
				
				if (ImGui::Button("Low Quality")) {
					ssr.MaxDistance = 50.0f;
					ssr.MaxSteps = 64;
					ssr.Resolution = 0.5f;
					ssr.Thickness = 0.5f;
				}
				ImGui::SameLine();
				if (ImGui::Button("Medium Quality")) {
					ssr.MaxDistance = 100.0f;
					ssr.MaxSteps = 128;
					ssr.Resolution = 0.75f;
					ssr.Thickness = 0.2f;
				}
				ImGui::SameLine();
				if (ImGui::Button("High Quality")) {
					ssr.MaxDistance = 200.0f;
					ssr.MaxSteps = 256;
					ssr.Resolution = 1.0f;
					ssr.Thickness = 0.1f;
				}
			}
		}
		
		ImGui::Spacing();
		
		// ========================================
		// SCREEN SPACE AMBIENT OCCLUSION (SSAO) - Placeholder
		// ========================================
		if (ImGui::CollapsingHeader("Ambient Occlusion (SSAO)")) {
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Coming soon...");
			
			// Future implementation
			// static bool ssaoEnabled = false;
			// ImGui::Checkbox("Enable SSAO", &ssaoEnabled);
			// ImGui::SliderFloat("Radius", ...);
			// ImGui::SliderFloat("Bias", ...);
			// ImGui::SliderInt("Samples", ...);
		}
		
		ImGui::Spacing();
		
		// ========================================
		// BLOOM - Placeholder
		// ========================================
		if (ImGui::CollapsingHeader("Bloom")) {
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Coming soon...");
		}
		
		ImGui::Spacing();
		
		// ========================================
		// TONE MAPPING - Placeholder
		// ========================================
		if (ImGui::CollapsingHeader("Tone Mapping")) {
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Coming soon...");
			// Options: ACES, Reinhard, Uncharted2, etc.
		}
	}
}
