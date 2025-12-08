#include "SettingsPanel.h"
#include "imgui.h"
#include "Renderer/SkyboxRenderer.h"
#include "Log/Log.h"
#include "ContentBrowserPanel.h"  // For ContentBrowserPayload

#include <filesystem>

namespace Lunex {
	void SettingsPanel::OnImGuiRender() {
		ImGui::Begin("Settings");
		
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
					// ContentBrowserPayload uses char[], not wchar_t*
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
					// Re-apply background clear color via SkyboxRenderer API
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
				// Background color (when no HDRI) - applies to clear color
				glm::vec3 bgColor = SkyboxRenderer::GetBackgroundColor();
				if (ImGui::ColorEdit3("Background Color", &bgColor.x)) {
					SkyboxRenderer::SetBackgroundColor(bgColor);
					// Update the clear color via SkyboxRenderer API (UI does not call RenderCommand directly)
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
			}
		}
		
		ImGui::Separator();
		
		// ========================================
		// PHYSICS 2D
		// ========================================
		if (ImGui::CollapsingHeader("Physics 2D")) {
			ImGui::Checkbox("Show 2D colliders", &m_ShowPhysicsColliders);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Display Box2D colliders in red");
			}
		}
		
		// ========================================
		// PHYSICS 3D
		// ========================================
		if (ImGui::CollapsingHeader("Physics 3D")) {
			ImGui::Checkbox("Show 3D colliders", &m_ShowPhysics3DColliders);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Display Bullet3D colliders in green");
			}
		}
		
		ImGui::End();
	}
}
