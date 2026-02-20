/**
 * @file SettingsPanel.cpp
 * @brief Settings Panel Implementation using Lunex UI Framework
 */

#include "stpch.h"
#include "SettingsPanel.h"
#include "ContentBrowserPanel.h"

#include "Core/Application.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/PostProcess/PostProcessRenderer.h"
#include "Rendering/RenderSystem.h"
#include "Scene/Lighting/LightSystem.h"
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsConfig.h"
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
		
		if (BeginTabBar("SettingsTabs")) {
			if (BeginTabItem("Render")) {
				DrawRenderTab();
				EndTabItem();
			}
			if (BeginTabItem("Post-Process")) {
				DrawPostProcessSection();
				EndTabItem();
			}
			if (BeginTabItem("Environment")) {
				DrawEnvironmentTab();
				EndTabItem();
			}
			if (BeginTabItem("Shadows")) {
				DrawShadowsTab();
				EndTabItem();
			}
			if (BeginTabItem("Physics")) {
				DrawPhysicsTab();
				EndTabItem();
			}
			EndTabBar();
		}
		
		EndPanel();
	}
	
	// ========================================
	// RENDER TAB
	// ========================================
	
	void SettingsPanel::DrawRenderTab() {
		using namespace UI;
		
		DrawRenderSection();
	}
	
	// ========================================
	// ENVIRONMENT TAB
	// ========================================
	
	void SettingsPanel::DrawEnvironmentTab() {
		DrawEnvironmentSection();
	}
	
	// ========================================
	// SHADOWS TAB
	// ========================================
	
	void SettingsPanel::DrawShadowsTab() {
		DrawShadowsSection();
	}
	
	// ========================================
	// RENDER SECTION
	// ========================================
	
	void SettingsPanel::DrawRenderSection() {
		using namespace UI;
		
		if (BeginSection("Renderer", true)) {
			auto& config = RenderSystem::GetConfig();
			
			// ========================================
			// QUALITY SETTINGS
			// ========================================
			Text("Quality");
			AddSpacing(SpacingValues::XS);
			
			if (PropertyCheckbox("VSync", config.EnableVSync, "Enable vertical synchronization")) {
				auto& window = Application::Get().GetWindow();
				window.SetVSync(config.EnableVSync);
			}
			
			if (PropertyCheckbox("HDR Rendering", config.EnableHDR, "Enable High Dynamic Range rendering")) {
			}
			
			if (config.EnableHDR) {
				if (PropertyFloat("Exposure", config.Exposure, 0.01f, 0.01f, 10.0f, "Tone mapping exposure")) {
				}
			}
			
			if (PropertyCheckbox("MSAA", config.EnableMSAA, "Enable Multi-Sample Anti-Aliasing")) {
			}
			
			if (config.EnableMSAA) {
				int msaaIdx = 0;
				const char* msaaOptions[] = { "2x", "4x", "8x" };
				int msaaValues[] = { 2, 4, 8 };
				for (int i = 0; i < 3; i++) {
					if (msaaValues[i] == (int)config.MSAASamples) { msaaIdx = i; break; }
				}
				if (PropertyDropdown("MSAA Samples", msaaIdx, msaaOptions, 3, "Number of MSAA samples")) {
					config.MSAASamples = msaaValues[msaaIdx];
				}
			}
			
			Separator();
			
			// ========================================
			// POST-PROCESSING (basic toggle - details in Post-Process tab)
			// ========================================
			Text("Post-Processing");
			AddSpacing(SpacingValues::XS);
			
			auto& ppConfig = PostProcessRenderer::GetConfig();
			
			if (PropertyCheckbox("Bloom", ppConfig.EnableBloom, "Enable bloom effect (configure in Post-Process tab)")) {
				config.EnableBloom = ppConfig.EnableBloom;
			}
			
			if (PropertyCheckbox("SSAO", config.EnableSSAO, "Enable Screen-Space Ambient Occlusion")) {
			}
			
			Separator();
			
			// ========================================
			// GRID SETTINGS
			// ========================================
			Text("Editor Grid");
			AddSpacing(SpacingValues::XS);
			
			auto& gridSettings = GridRenderer::GetSettings();
			
			if (PropertyColor("Grid Color", gridSettings.GridColor)) {
			}
			
			if (PropertyFloat("Grid Scale", gridSettings.GridScale, 0.1f, 0.1f, 100.0f, "Size of each grid cell in units")) {
			}
			
			if (PropertyFloat("Grid Extent", gridSettings.FadeDistance, 1.0f, 5.0f, 500.0f, "How far the grid extends")) {
			}
			
			if (PropertyFloat("Minor Thickness", gridSettings.MinorLineThickness, 0.1f, 0.1f, 5.0f, "Minor grid line thickness")) {
			}
			
			if (PropertyFloat("Major Thickness", gridSettings.MajorLineThickness, 0.1f, 0.1f, 5.0f, "Major grid line thickness (every 10 lines)")) {
			}
			
			Separator();
			
			// ========================================
			// PERFORMANCE
			// ========================================
			Text("Performance");
			AddSpacing(SpacingValues::XS);
			
			bool parallelPasses = RenderSystem::IsParallelPassesEnabled();
			if (PropertyCheckbox("Parallel Passes", parallelPasses, "Enable parallel render pass execution")) {
				RenderSystem::SetParallelPassesEnabled(parallelPasses);
			}
			
			if (PropertyCheckbox("Parallel Draw Collection", config.EnableParallelDrawCollection, "Enable parallel entity iteration for draw commands")) {
			}
			
			EndSection();
		}
	}
	
	// ========================================
	// POST-PROCESS SECTION
	// ========================================
	
	void SettingsPanel::DrawPostProcessSection() {
		using namespace UI;
		
		if (BeginSection("Post-Processing", true)) {
			auto& ppConfig = PostProcessRenderer::GetConfig();
			auto& renderConfig = RenderSystem::GetConfig();
			
			if (!PostProcessRenderer::IsInitialized()) {
				TextColored(Colors::Warning(), "Post-processing not initialized (requires Deferred Rendering)");
				EndSection();
				return;
			}
			
			// ========================================
			// BLOOM
			// ========================================
			Text("Bloom");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("Bloom simulates bright light bleeding beyond object boundaries, creating a glow effect.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			if (PropertyCheckbox("Enable Bloom", ppConfig.EnableBloom, "Enable bloom post-processing effect")) {
				renderConfig.EnableBloom = ppConfig.EnableBloom;
			}
			
			if (ppConfig.EnableBloom) {
				if (PropertyFloat("Threshold", ppConfig.BloomThreshold, 0.05f, 0.0f, 5.0f,
					"Brightness threshold for bloom extraction (lower = more glow)")) {
					renderConfig.BloomThreshold = ppConfig.BloomThreshold;
				}
				
				if (PropertyFloat("Intensity", ppConfig.BloomIntensity, 0.01f, 0.0f, 3.0f,
					"Bloom contribution to the final image")) {
					renderConfig.BloomIntensity = ppConfig.BloomIntensity;
				}
				
				if (PropertyFloat("Radius", ppConfig.BloomRadius, 0.1f, 0.1f, 5.0f,
					"Blur kernel spread (higher = wider glow)")) {
					renderConfig.BloomRadius = ppConfig.BloomRadius;
				}
				
				float mipLevelsF = static_cast<float>(ppConfig.BloomMipLevels);
				if (PropertyFloat("Quality (Mip Levels)", mipLevelsF, 1.0f, 1.0f, 8.0f,
					"Number of downsample passes (more = smoother bloom, slightly slower)")) {
					ppConfig.BloomMipLevels = static_cast<int>(mipLevelsF);
				}
			}
			
			Separator();
			
			// ========================================
			// VIGNETTE
			// ========================================
			Text("Vignette");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("Darkens the edges of the screen, drawing focus to the center.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			if (PropertyCheckbox("Enable Vignette", ppConfig.EnableVignette, "Enable vignette effect")) {
				renderConfig.EnableVignette = ppConfig.EnableVignette;
			}
			
			if (ppConfig.EnableVignette) {
				if (PropertySlider("Intensity##Vignette", ppConfig.VignetteIntensity, 0.0f, 1.0f, "%.2f",
					"How much the edges darken")) {
					renderConfig.VignetteIntensity = ppConfig.VignetteIntensity;
				}
				
				if (PropertySlider("Roundness", ppConfig.VignetteRoundness, 0.0f, 2.0f, "%.2f",
					"Shape roundness (1.0 = circular, lower = more rectangular)")) {
					renderConfig.VignetteRoundness = ppConfig.VignetteRoundness;
				}
				
				if (PropertySlider("Smoothness", ppConfig.VignetteSmoothness, 0.01f, 1.0f, "%.2f",
					"Transition smoothness from center to edges")) {
					renderConfig.VignetteSmoothness = ppConfig.VignetteSmoothness;
				}
			}
			
			Separator();
			
			// ========================================
			// CHROMATIC ABERRATION
			// ========================================
			Text("Chromatic Aberration");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("Simulates lens color fringing by splitting RGB channels at the edges.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			if (PropertyCheckbox("Enable Chromatic Aberration", ppConfig.EnableChromaticAberration,
				"Enable chromatic aberration effect")) {
				renderConfig.EnableChromaticAberration = ppConfig.EnableChromaticAberration;
			}
			
			if (ppConfig.EnableChromaticAberration) {
				if (PropertyFloat("Intensity##ChromAb", ppConfig.ChromaticAberrationIntensity, 0.001f, 0.0f, 0.05f,
					"Amount of color channel separation")) {
					renderConfig.ChromaticAberrationIntensity = ppConfig.ChromaticAberrationIntensity;
				}
			}
			
			Separator();
			
			// ========================================
			// TONE MAPPING
			// ========================================
			Text("Tone Mapping");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("Controls how HDR values are mapped to displayable range.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			const char* toneMapOptions[] = { "ACES Film", "Reinhard", "Uncharted 2", "None (Linear)" };
			if (PropertyDropdown("Operator", ppConfig.ToneMapOperator, toneMapOptions, 4,
				"Tone mapping algorithm")) {
				renderConfig.ToneMapOperator = ppConfig.ToneMapOperator;
			}
			
			if (PropertyFloat("Exposure##PP", ppConfig.Exposure, 0.01f, 0.01f, 10.0f,
				"Exposure multiplier before tone mapping")) {
				renderConfig.Exposure = ppConfig.Exposure;
			}
			
			if (PropertyFloat("Gamma", ppConfig.Gamma, 0.01f, 1.0f, 3.0f,
				"Gamma correction value (2.2 = standard sRGB)")) {
			}
			
			EndSection();
		}
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
	
	void SettingsPanel::DrawShadowsSection() {
		using namespace UI;
		
		if (BeginSection("Shadows", true)) {
			auto& shadowSystem = ShadowSystem::Get();
			ShadowConfig config = shadowSystem.GetConfig();
			bool configChanged = false;
			
			// ========================================
			// GLOBAL SHADOW SETTINGS
			// ========================================
			bool shadowsEnabled = shadowSystem.IsEnabled();
			if (PropertyCheckbox("Enable Shadows", shadowsEnabled)) {
				shadowSystem.SetEnabled(shadowsEnabled);
			}
			
			if (!shadowsEnabled) {
				TextWrapped("Shadows are disabled globally.", TextVariant::Muted);
				EndSection();
				return;
			}
			
			Separator();
			
			// ========================================
			// DIRECTIONAL / SUN LIGHT SHADOWS (CSM)
			// ========================================
			Text("Directional Light (CSM)");
			AddSpacing(SpacingValues::XS);
			
			bool hasSunLight = LightSystem::Get().HasSunLight();
			if (hasSunLight) {
				TextColored(Colors::Success(), "Sun Light detected ~ CSM active");
			} else {
				uint32_t dirCount = LightSystem::Get().GetDirectionalLightCount();
				if (dirCount > 0) {
					TextColored(Colors::Info(), "%d Directional Light(s) ~ CSM active", dirCount);
				} else {
					TextColored(Colors::TextMuted(), "No directional lights in scene");
				}
			}
			
			AddSpacing(SpacingValues::XS);
			
			// CSM Resolution
			int dirRes = static_cast<int>(config.DirectionalResolution);
			const char* resOptions[] = { "512", "1024", "2048", "4096" };
			int resValues[] = { 512, 1024, 2048, 4096 };
			int resIdx = 2; // default 2048
			for (int i = 0; i < 4; i++) {
				if (resValues[i] == dirRes) { resIdx = i; break; }
			}
			if (PropertyDropdown("Resolution", resIdx, resOptions, 4, "Shadow map resolution per cascade")) {
				config.DirectionalResolution = resValues[resIdx];
				configChanged = true;
			}
			
			// Cascade count
			const char* cascadeOptions[] = { "1", "2", "3", "4" };
			int cascadeIdx = static_cast<int>(config.CSMCascadeCount) - 1;
			cascadeIdx = glm::clamp(cascadeIdx, 0, 3);
			if (PropertyDropdown("Cascade Count", cascadeIdx, cascadeOptions, 4, "Number of shadow cascades (1-4)")) {
				config.CSMCascadeCount = static_cast<uint32_t>(cascadeIdx + 1);
				configChanged = true;
			}
			
			// Max shadow distance
			if (PropertyFloat("Max Distance", config.MaxShadowDistance, 1.0f, 10.0f, 1000.0f, "Maximum distance for directional shadows")) {
				configChanged = true;
			}
			
			// Split lambda
			if (PropertySlider("Split Lambda", config.CSMSplitLambda, 0.0f, 1.0f, "%.2f", "0 = linear splits, 1 = logarithmic splits")) {
				configChanged = true;
			}
			
			// Directional bias
			if (PropertyFloat("Bias", config.DirectionalBias, 0.0001f, 0.0f, 0.05f, "Depth bias for directional shadows")) {
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// SPOT & POINT LIGHT SHADOWS
			// ========================================
			Text("Spot & Point Light Shadows");
			AddSpacing(SpacingValues::XS);
			
			uint32_t spotCount = LightSystem::Get().GetSpotLightCount();
			uint32_t pointCount = LightSystem::Get().GetPointLightCount();
			Text("  Spot Lights: %d  |  Point Lights: %d", spotCount, pointCount);
			
			AddSpacing(SpacingValues::XS);
			
			// Spot resolution
			int spotRes = static_cast<int>(config.SpotResolution);
			int spotResIdx = 1; // default 1024
			for (int i = 0; i < 4; i++) {
				if (resValues[i] == spotRes) { spotResIdx = i; break; }
			}
			if (PropertyDropdown("Spot Resolution", spotResIdx, resOptions, 4, "Shadow map resolution for spot lights")) {
				config.SpotResolution = resValues[spotResIdx];
				configChanged = true;
			}
			
			// Point resolution
			int pointRes = static_cast<int>(config.PointResolution);
			int pointResIdx = 0; // default 512
			for (int i = 0; i < 4; i++) {
				if (resValues[i] == pointRes) { pointResIdx = i; break; }
			}
			if (PropertyDropdown("Point Resolution", pointResIdx, resOptions, 4, "Shadow map resolution per cubemap face")) {
				config.PointResolution = resValues[pointResIdx];
				configChanged = true;
			}
			
			// Spot bias
			if (PropertyFloat("Spot Bias", config.SpotBias, 0.0001f, 0.0f, 0.1f, "Depth bias for spot light shadows")) {
				configChanged = true;
			}
			
			// Point bias
			if (PropertyFloat("Point Bias", config.PointBias, 0.001f, 0.0f, 0.5f, "Depth bias for point light shadows")) {
                configChanged = true;
            }
            
			Separator();
			
			// ========================================
			// FILTERING & SOFTENING
			// ========================================
			Text("Filtering & Softening");
			AddSpacing(SpacingValues::XS);
			
			// PCF Radius
			if (PropertyFloat("PCF Radius", config.PCFRadius, 0.1f, 0.0f, 8.0f, "Base radius for Poisson disk PCF sampling")) {
				configChanged = true;
			}
			
			// Distance-based softening
			if (PropertyFloat("Soften Start Dist", config.DistanceSofteningStart, 1.0f, 0.0f, 500.0f,
				"Distance from camera where shadows begin to soften")) {
				configChanged = true;
			}
			
			if (PropertyFloat("Soften Max", config.DistanceSofteningMax, 0.1f, 1.0f, 10.0f,
				"Maximum PCF radius multiplier at far distance")) {
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// SKY COLOR TINTING
			// ========================================
			Text("Shadow Color Tinting");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("Shadows in the real world receive indirect light from the sky, giving them a subtle color tint.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			if (PropertyCheckbox("Enable Sky Tint", config.EnableSkyColorTint, "Tint shadows with sky/environment color")) {
				configChanged = true;
			}
			
			if (config.EnableSkyColorTint) {
				if (PropertySlider("Tint Strength", config.SkyTintStrength, 0.0f, 0.5f, "%.2f",
					"How much sky color bleeds into shadowed areas")) {
					configChanged = true;
				}
			}
			
			Separator();
			
			// ========================================
			// SHADOW STATISTICS
			// ========================================
			Text("Shadow Statistics");
			AddSpacing(SpacingValues::XS);
			
			auto stats = shadowSystem.GetStatistics();
			TextColored(Colors::TextSecondary(), "  Maps Rendered: %d", stats.ShadowMapsRendered);
			TextColored(Colors::TextSecondary(), "  Cascades: %d", stats.CascadesRendered);
			TextColored(Colors::TextSecondary(), "  Spot Maps: %d", stats.SpotMapsRendered);
			TextColored(Colors::TextSecondary(), "  Point Faces: %d", stats.PointFacesRendered);
			TextColored(Colors::TextSecondary(), "  Draw Calls: %d", stats.ShadowDrawCalls);
			
			// Apply config changes
			if (configChanged) {
				shadowSystem.SetConfig(config);
			}
			
			EndSection();
		}
	}
	
	void SettingsPanel::DrawPhysicsTab() {
		DrawPhysicsGeneralSection();
		
		DrawPhysics2DSection();
		
		DrawPhysics3DSection();
	}
	
	void SettingsPanel::DrawPhysicsGeneralSection() {
		using namespace UI;
		
		if (BeginSection("General Physics", true)) {
			auto& physicsCore = PhysicsCore::Get();
			PhysicsConfig config = physicsCore.GetConfig();
			bool configChanged = false;
			
			// ======================================
			// GRAVITY
			// ======================================
			Text("World Gravity");
			AddSpacing(SpacingValues::XS);
			
			glm::vec3 gravity = config.Gravity;
			if (PropertyFloat("Gravity X", gravity.x, 0.1f, -100.0f, 100.0f, "Horizontal gravity (m/s^2)")) {
				config.Gravity.x = gravity.x;
				configChanged = true;
			}
			if (PropertyFloat("Gravity Y", gravity.y, 0.1f, -100.0f, 100.0f, "Vertical gravity (m/s^2), -9.81 = Earth")) {
				config.Gravity.y = gravity.y;
				configChanged = true;
			}
			if (PropertyFloat("Gravity Z", gravity.z, 0.1f, -100.0f, 100.0f, "Depth gravity (m/s^2)")) {
				config.Gravity.z = gravity.z;
				configChanged = true;
			}
			
			// Gravity presets
			AddSpacing(SpacingValues::XS);
			Text("Presets:");
			AddSpacing(SpacingValues::XS);
			
			if (Button("Earth", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, -9.81f, 0.0f);
				configChanged = true;
			}
			ImGui::SameLine();
			if (Button("Moon", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, -1.62f, 0.0f);
				configChanged = true;
			}
			ImGui::SameLine();
			if (Button("Mars", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, -3.72f, 0.0f);
				configChanged = true;
			}
			ImGui::SameLine();
			if (Button("Zero-G", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, 0.0f, 0.0f);
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// SIMULATION SETTINGS
			// ========================================
			Text("Simulation");
			AddSpacing(SpacingValues::XS);
			
			const char* timestepOptions[] = { "30 Hz", "60 Hz", "120 Hz", "240 Hz" };
			float timestepValues[] = { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f, 1.0f / 240.0f };
			int timestepIdx = 1;
			for (int i = 0; i < 4; i++) {
				if (std::abs(timestepValues[i] - config.FixedTimestep) < 0.0001f) { timestepIdx = i; break; }
			}
			if (PropertyDropdown("Fixed Timestep", timestepIdx, timestepOptions, 4, "Physics simulation frequency")) {
				config.FixedTimestep = timestepValues[timestepIdx];
				configChanged = true;
			}
			
			float maxSubStepsF = static_cast<float>(config.MaxSubSteps);
			if (PropertyFloat("Max Substeps", maxSubStepsF, 1.0f, 1.0f, 60.0f, "Maximum physics substeps per frame")) {
				config.MaxSubSteps = static_cast<int>(maxSubStepsF);
				configChanged = true;
			}
			
			float solverIterF = static_cast<float>(config.SolverIterations);
			if (PropertyFloat("Solver Iterations", solverIterF, 1.0f, 1.0f, 100.0f, "Higher = more stable stacking, slower")) {
				config.SolverIterations = static_cast<int>(solverIterF);
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// CCD (Continuous Collision Detection)
			// ========================================
			Text("Continuous Collision Detection");
			AddSpacing(SpacingValues::XS);
			
			TextWrapped("CCD prevents fast-moving objects from tunneling through thin surfaces.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);
			
			if (PropertyFloat("Motion Threshold", config.DefaultCcdMotionThreshold, 0.01f, 0.0f, 10.0f,
				"Objects moving faster than this (m/frame) use CCD")) {
				configChanged = true;
			}
			
			if (PropertyFloat("Swept Sphere Radius", config.DefaultCcdSweptSphereRadius, 0.01f, 0.0f, 5.0f,
				"Radius for CCD swept sphere test")) {
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// DEBUG
			// ========================================
			Text("Debug");
			AddSpacing(SpacingValues::XS);
			
			bool debugDraw = config.EnableDebugDraw;
			if (PropertyCheckbox("Debug Draw", debugDraw, "Render physics debug wireframes")) {
				config.EnableDebugDraw = debugDraw;
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// STATISTICS
			// ========================================
			Text("Statistics");
		 AddSpacing(SpacingValues::XS);
			
			auto* world = physicsCore.GetWorld();
			if (world) {
				TextColored(Colors::TextSecondary(), "  Rigid Bodies: %d", world->GetNumRigidBodies());
				TextColored(Colors::TextSecondary(), "  Contact Manifolds: %d", world->GetNumManifolds());
				TextColored(Colors::TextSecondary(), "  Sim Steps (total): %d", physicsCore.GetSimulationSteps());
			} else {
				TextColored(Colors::TextMuted(), "  Physics not initialized");
			}
			
			if (configChanged) {
				physicsCore.SetConfig(config);
			}
			
			EndSection();
		}
	}
	
	void SettingsPanel::DrawPhysics2DSection() {
		using namespace UI;
		
		if (BeginSection("Physics 2D", false)) {
			if (PropertyCheckbox("Show 2D colliders", m_ShowPhysicsColliders, "Display Box2D colliders")) {
				// Value updated by reference
			}
			
			EndSection();
		}
	}
	
	void SettingsPanel::DrawPhysics3DSection() {
		using namespace UI;
		
		if (BeginSection("Physics 3D", false)) {
			if (PropertyCheckbox("Show 3D colliders", m_ShowPhysics3DColliders, "Display Bullet3D colliders")) {
				// Value updated by reference
			}
			
			EndSection();
		}
	}
}
