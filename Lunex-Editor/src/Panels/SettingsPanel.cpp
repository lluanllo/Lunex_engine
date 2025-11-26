#include "SettingsPanel.h"
#include "imgui.h"

namespace Lunex {
	void SettingsPanel::OnImGuiRender() {
		ImGui::Begin("Settings");
		
		// Physics Settings
		if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			ImGui::Checkbox("Show physics colliders", &m_ShowPhysicsColliders);
			ImGui::Unindent();
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// ===== COMPUTE RAY TRACING =====
		if (ImGui::CollapsingHeader("Compute Ray Tracing (Experimental)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			
			ImGui::Checkbox("Enable Compute Ray Tracing", &m_EnableComputeRayTracing);
			
			if (m_EnableComputeRayTracing) {
				ImGui::TextWrapped("Uses GPU compute shaders for accurate shadows with BVH acceleration.");
				ImGui::Spacing();
				ImGui::Text("Note: Requires scene geometry rebuild when objects change.");
			}
			
			ImGui::Unindent();
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// Ray Tracing Settings
		if (ImGui::CollapsingHeader("Ray Traced Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			
			auto& rtSettings = Renderer3D::GetRayTracingSettings();
			bool changed = false;
			
			// Enable/Disable RT Shadows
			changed |= ImGui::Checkbox("Enable Ray Traced Shadows", &rtSettings.EnableRayTracedShadows);
			
			if (rtSettings.EnableRayTracedShadows) {
				ImGui::Spacing();
				
				// Shadow Quality
				if (ImGui::TreeNodeEx("Shadow Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
					changed |= ImGui::SliderInt("Samples Per Light", &rtSettings.ShadowSamplesPerLight, 1, 16);
					ImGui::SameLine();
					if (ImGui::Button("?")) {
						ImGui::SetTooltip("Higher = softer shadows, lower performance");
					}
					
					changed |= ImGui::SliderFloat("Shadow Softness", &rtSettings.ShadowSoftness, 0.0f, 2.0f, "%.2f");
					changed |= ImGui::SliderFloat("Shadow Bias", &rtSettings.ShadowRayBias, 0.0001f, 0.01f, "%.4f");
					
					ImGui::TreePop();
				}
				
				ImGui::Spacing();
				
				// Distance Settings
				if (ImGui::TreeNodeEx("Distance & Fading", ImGuiTreeNodeFlags_DefaultOpen)) {
					changed |= ImGui::SliderFloat("Max Shadow Distance", &rtSettings.MaxShadowDistance, 10.0f, 500.0f, "%.1f");
					changed |= ImGui::SliderFloat("Fade Distance", &rtSettings.ShadowFadeDistance, 1.0f, 100.0f, "%.1f");
					
					ImGui::TreePop();
				}
				
				ImGui::Spacing();
				
				// Advanced Settings
				if (ImGui::TreeNode("Advanced")) {
					changed |= ImGui::Checkbox("Contact Hardening", &rtSettings.UseContactHardening);
					if (rtSettings.UseContactHardening) {
						ImGui::Indent();
						changed |= ImGui::SliderFloat("Hardening Scale", &rtSettings.ContactHardeningScale, 0.1f, 10.0f, "%.2f");
						ImGui::Unindent();
					}
					
					ImGui::Spacing();
					
					changed |= ImGui::Checkbox("Enable Ground Plane", &rtSettings.EnableGroundPlane);
					if (rtSettings.EnableGroundPlane) {
						ImGui::Indent();
						changed |= ImGui::SliderFloat("Ground Y Position", &rtSettings.GroundPlaneY, -10.0f, 10.0f, "%.2f");
						ImGui::Unindent();
					}
					
					ImGui::Spacing();
					changed |= ImGui::Checkbox("Visualize Shadow Rays", &rtSettings.VisualizeShadowRays);
					
					ImGui::TreePop();
				}
				
				ImGui::Spacing();
				
				// Presets
				if (ImGui::TreeNode("Presets")) {
					if (ImGui::Button("Low Quality (Fast)", ImVec2(200, 0))) {
						rtSettings.ShadowSamplesPerLight = 1;
						rtSettings.ShadowSoftness = 0.2f;
						rtSettings.UseContactHardening = false;
						changed = true;
					}
					
					if (ImGui::Button("Medium Quality", ImVec2(200, 0))) {
						rtSettings.ShadowSamplesPerLight = 4;
						rtSettings.ShadowSoftness = 0.5f;
						rtSettings.UseContactHardening = true;
						rtSettings.ContactHardeningScale = 1.0f;
						changed = true;
					}
					
					if (ImGui::Button("High Quality (Slow)", ImVec2(200, 0))) {
						rtSettings.ShadowSamplesPerLight = 8;
						rtSettings.ShadowSoftness = 1.0f;
						rtSettings.UseContactHardening = true;
						rtSettings.ContactHardeningScale = 2.0f;
						changed = true;
					}
					
					if (ImGui::Button("Ultra Quality", ImVec2(200, 0))) {
						rtSettings.ShadowSamplesPerLight = 16;
						rtSettings.ShadowSoftness = 1.5f;
						rtSettings.UseContactHardening = true;
						rtSettings.ContactHardeningScale = 3.0f;
						changed = true;
					}
					
					ImGui::TreePop();
				}
			}
			
			// Update GPU buffer if settings changed
			if (changed) {
				Renderer3D::UpdateRayTracingBuffer();
			}
			
			ImGui::Unindent();
		}
		
		ImGui::End();
	}
}
