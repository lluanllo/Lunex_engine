/**
 * @file SettingsPanel.cpp
 * @brief Settings Panel Implementation using Lunex UI Framework — Tab layout
 */

#include "stpch.h"
#include "SettingsPanel.h"
#include "ContentBrowserPanel.h"

#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
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

		if (BeginTabBar("SettingsTabs")) {
			if (BeginTabItem("Render")) {
				DrawRenderTab();
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

	// ====================================================================
	// RENDER TAB (NEW)
	// ====================================================================

	void SettingsPanel::DrawRenderTab() {
		using namespace UI;

		AddSpacing(SpacingValues::SM);

		// ?? Backend selector ????????????????????????????????????????????
		Text("Render Engine");
		AddSpacing(SpacingValues::XS);

		int currentBackend = m_RenderSystem
			? static_cast<int>(m_RenderSystem->GetActiveBackendType())
			: 0;
		const char* backendOptions[] = { "Rasterizer", "Path Tracer" };

		if (PropertyDropdown("Backend", currentBackend, backendOptions, 2,
		                     "Choose the 3D rendering backend")) {
			if (m_RenderSystem) {
				m_RenderSystem->SetActiveBackend(
					static_cast<RenderBackendType>(currentBackend));
			}
		}

		Separator();

		// ?? Per-backend settings ????????????????????????????????????????
		if (m_RenderSystem && m_RenderSystem->GetActiveBackendType() == RenderBackendType::PathTracer) {
			Text("Path Tracer Settings");
			AddSpacing(SpacingValues::XS);

			auto* backend = m_RenderSystem->GetActiveBackend();
			RenderBackendSettings settings = backend->GetSettings();
			bool changed = false;

			int bounces = static_cast<int>(settings.MaxBounces);
			if (PropertyInt("Max Bounces", bounces, 1, 1, 16,
			                "Maximum light bounces per ray")) {
				settings.MaxBounces = static_cast<uint32_t>(bounces);
				changed = true;
			}

			int spp = static_cast<int>(settings.SamplesPerFrame);
			if (PropertyInt("Samples / Frame", spp, 1, 1, 16,
			                "Samples accumulated per frame")) {
				settings.SamplesPerFrame = static_cast<uint32_t>(spp);
				changed = true;
			}

			int maxSamples = static_cast<int>(settings.MaxAccumulatedSamples);
			if (PropertyInt("Max Samples", maxSamples, 0, 0, 100000,
			                "0 = infinite accumulation")) {
				settings.MaxAccumulatedSamples = static_cast<uint32_t>(maxSamples);
				changed = true;
			}

			if (PropertyFloat("Russian Roulette", settings.RussianRouletteThresh,
			                  0.001f, 0.0f, 1.0f,
			                  "Probability threshold for early ray termination")) {
				changed = true;
			}

			if (changed) backend->SetSettings(settings);

			Separator();

			// ?? Accumulation info ????????????????????????????????????
			Text("Accumulation");
			AddSpacing(SpacingValues::XS);

			auto stats = backend->GetStats();
			TextColored(Colors::TextSecondary(), "  Accumulated Samples: %d", stats.AccumulatedSamples);
			TextColored(Colors::TextSecondary(), "  Triangles (BVH):     %d", stats.TotalTriangles);
			TextColored(Colors::TextSecondary(), "  BVH Nodes:           %d", stats.BVHNodeCount);

			AddSpacing(SpacingValues::SM);
			if (Button("Reset Accumulation", ButtonVariant::Secondary)) {
				backend->ResetAccumulation();
			}
		}
		else {
			Text("Rasterizer Settings");
			AddSpacing(SpacingValues::XS);
			TextColored(Colors::TextMuted(), "Using the standard forward PBR pipeline.");
			TextColored(Colors::TextMuted(), "Shadow and lighting settings are in their respective tabs.");

			if (m_RenderSystem) {
				auto stats = m_RenderSystem->GetActiveBackend()->GetStats();
				AddSpacing(SpacingValues::SM);
				TextColored(Colors::TextSecondary(), "  Draw Calls: %d", stats.DrawCalls);
				TextColored(Colors::TextSecondary(), "  Triangles:  %d", stats.TriangleCount);
				TextColored(Colors::TextSecondary(), "  Meshes:     %d", stats.MeshCount);
			}
		}
	}

	// ====================================================================
	// ENVIRONMENT TAB (was DrawEnvironmentSection)
	// ====================================================================
	
	void SettingsPanel::DrawEnvironmentTab() {
		using namespace UI;
		
		AddSpacing(SpacingValues::SM);

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
		
		// Drop target for HDRI files
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
		
		// Sun Light Synchronization
		Text("Sun Light Synchronization");
		AddSpacing(SpacingValues::XS);
		
		bool hasSunLight = LightSystem::Get().HasSunLight();
		bool syncWithSun = SkyboxRenderer::IsSyncWithSunLight();
		
		if (hasSunLight) {
			TextColored(Colors::Success(), "Sun Light: Active");
			
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
		
		// Environment settings
		if (hasHDRI) {
			float intensity = SkyboxRenderer::GetIntensity();
			if (PropertyFloat("Intensity", intensity, 0.01f, 0.0f, 10.0f)) {
				SkyboxRenderer::SetIntensity(intensity);
			}
			
			if (syncWithSun) {
				float rotation = SkyboxRenderer::GetCalculatedSkyboxRotation();
				BeginDisabled(true);
				PropertyFloat("Rotation (Synced)", rotation, 1.0f, -180.0f, 180.0f);
				EndDisabled();
				if (IsItemHovered()) {
					SetTooltip("Rotation is controlled by the Sun Light.\nDisable 'Link to Skybox' in the light component to use manual rotation.");
				}
			} else {
				float rotation = SkyboxRenderer::GetRotation();
				if (PropertyFloat("Rotation", rotation, 1.0f, -180.0f, 180.0f)) {
					SkyboxRenderer::SetRotation(rotation);
				}
			}
			
			float blur = SkyboxRenderer::GetBlur();
			if (PropertySlider("Blur", blur, 0.0f, 1.0f)) {
				SkyboxRenderer::SetBlur(blur);
			}
			
			glm::vec3 tint = SkyboxRenderer::GetTint();
			if (PropertyColor("Tint", tint)) {
				SkyboxRenderer::SetTint(tint);
			}
		} else {
			glm::vec3 bgColor = SkyboxRenderer::GetBackgroundColor();
			
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
	}
	
	// ====================================================================
	// SHADOWS TAB (was DrawShadowsSection)
	// ====================================================================
	
	void SettingsPanel::DrawShadowsTab() {
		using namespace UI;
		
		AddSpacing(SpacingValues::SM);

		auto& shadowSystem = ShadowSystem::Get();
		ShadowConfig config = shadowSystem.GetConfig();
		bool configChanged = false;
		
		bool shadowsEnabled = shadowSystem.IsEnabled();
		if (PropertyCheckbox("Enable Shadows", shadowsEnabled)) {
			shadowSystem.SetEnabled(shadowsEnabled);
		}
		
		if (!shadowsEnabled) {
			TextWrapped("Shadows are disabled globally.", TextVariant::Muted);
			return;
		}
		
		Separator();
		
		// Directional / CSM
		Text("Directional Light (CSM)");
		AddSpacing(SpacingValues::XS);
		
		bool hasSunLight = LightSystem::Get().HasSunLight();
		if (hasSunLight) {
			TextColored(Colors::Success(), "Sun Light detected - CSM active");
		} else {
			uint32_t dirCount = LightSystem::Get().GetDirectionalLightCount();
			if (dirCount > 0) {
				TextColored(Colors::Info(), "%d Directional Light(s) - CSM active", dirCount);
			} else {
				TextColored(Colors::TextMuted(), "No directional lights in scene");
			}
		}
		
		AddSpacing(SpacingValues::XS);
		
		int dirRes = static_cast<int>(config.DirectionalResolution);
		const char* resOptions[] = { "512", "1024", "2048", "4096" };
		int resValues[] = { 512, 1024, 2048, 4096 };
		int resIdx = 2;
		for (int i = 0; i < 4; i++) {
			if (resValues[i] == dirRes) { resIdx = i; break; }
		}
		if (PropertyDropdown("Resolution", resIdx, resOptions, 4, "Shadow map resolution per cascade")) {
			config.DirectionalResolution = resValues[resIdx];
			configChanged = true;
		}
		
		const char* cascadeOptions[] = { "1", "2", "3", "4" };
		int cascadeIdx = static_cast<int>(config.CSMCascadeCount) - 1;
		cascadeIdx = glm::clamp(cascadeIdx, 0, 3);
		if (PropertyDropdown("Cascade Count", cascadeIdx, cascadeOptions, 4, "Number of shadow cascades (1-4)")) {
			config.CSMCascadeCount = static_cast<uint32_t>(cascadeIdx + 1);
			configChanged = true;
		}
		
		if (PropertyFloat("Max Distance", config.MaxShadowDistance, 1.0f, 10.0f, 1000.0f, "Maximum distance for directional shadows")) {
			configChanged = true;
		}
		
		if (PropertySlider("Split Lambda", config.CSMSplitLambda, 0.0f, 1.0f, "%.2f", "0 = linear splits, 1 = logarithmic splits")) {
			configChanged = true;
		}
		
		if (PropertyFloat("Bias", config.DirectionalBias, 0.0001f, 0.0f, 0.05f, "Depth bias for directional shadows")) {
			configChanged = true;
		}
		
		Separator();
		
		// Spot & Point
		Text("Spot & Point Light Shadows");
		AddSpacing(SpacingValues::XS);
		
		uint32_t spotCount = LightSystem::Get().GetSpotLightCount();
		uint32_t pointCount = LightSystem::Get().GetPointLightCount();
		Text("  Spot Lights: %d  |  Point Lights: %d", spotCount, pointCount);
		
		AddSpacing(SpacingValues::XS);
		
		int spotRes = static_cast<int>(config.SpotResolution);
		int spotResIdx = 1;
		for (int i = 0; i < 4; i++) {
			if (resValues[i] == spotRes) { spotResIdx = i; break; }
		}
		if (PropertyDropdown("Spot Resolution", spotResIdx, resOptions, 4, "Shadow map resolution for spot lights")) {
			config.SpotResolution = resValues[spotResIdx];
			configChanged = true;
		}
		
		int pointRes = static_cast<int>(config.PointResolution);
		int pointResIdx = 0;
		for (int i = 0; i < 4; i++) {
			if (resValues[i] == pointRes) { pointResIdx = i; break; }
		}
		if (PropertyDropdown("Point Resolution", pointResIdx, resOptions, 4, "Shadow map resolution per cubemap face")) {
			config.PointResolution = resValues[pointResIdx];
			configChanged = true;
		}
		
		if (PropertyFloat("Spot Bias", config.SpotBias, 0.0001f, 0.0f, 0.1f, "Depth bias for spot light shadows")) {
			configChanged = true;
		}
		
		if (PropertyFloat("Point Bias", config.PointBias, 0.001f, 0.0f, 0.5f, "Depth bias for point light shadows")) {
			configChanged = true;
		}
		
		Separator();
		
		// Filtering
		Text("Filtering & Softening");
		AddSpacing(SpacingValues::XS);
		
		if (PropertyFloat("PCF Radius", config.PCFRadius, 0.1f, 0.0f, 8.0f, "Base radius for Poisson disk PCF sampling")) {
			configChanged = true;
		}
		
		if (PropertyFloat("Soften Start Dist", config.DistanceSofteningStart, 1.0f, 0.0f, 500.0f)) {
			configChanged = true;
		}
		
		if (PropertyFloat("Soften Max", config.DistanceSofteningMax, 0.1f, 1.0f, 10.0f)) {
			configChanged = true;
		}
		
		Separator();
		
		// Sky tint
		Text("Shadow Color Tinting");
		AddSpacing(SpacingValues::XS);
		
		TextWrapped("Shadows in the real world receive indirect light from the sky.", TextVariant::Muted);
		AddSpacing(SpacingValues::XS);
		
		if (PropertyCheckbox("Enable Sky Tint", config.EnableSkyColorTint)) {
			configChanged = true;
		}
		
		if (config.EnableSkyColorTint) {
			if (PropertySlider("Tint Strength", config.SkyTintStrength, 0.0f, 0.5f, "%.2f")) {
				configChanged = true;
			}
		}
		
		Separator();
		
		// Stats
		Text("Shadow Statistics");
		AddSpacing(SpacingValues::XS);
		
		auto stats = shadowSystem.GetStatistics();
		TextColored(Colors::TextSecondary(), "  Maps Rendered: %d", stats.ShadowMapsRendered);
		TextColored(Colors::TextSecondary(), "  Cascades: %d", stats.CascadesRendered);
		TextColored(Colors::TextSecondary(), "  Spot Maps: %d", stats.SpotMapsRendered);
		TextColored(Colors::TextSecondary(), "  Point Faces: %d", stats.PointFacesRendered);
		TextColored(Colors::TextSecondary(), "  Draw Calls: %d", stats.ShadowDrawCalls);
		
		if (configChanged) {
			shadowSystem.SetConfig(config);
		}
	}
	
	// ====================================================================
	// PHYSICS TAB (combined 2D + 3D)
	// ====================================================================
	
	void SettingsPanel::DrawPhysicsTab() {
		using namespace UI;
		
		AddSpacing(SpacingValues::SM);

		Text("Physics 2D");
		AddSpacing(SpacingValues::XS);
		PropertyCheckbox("Show 2D colliders", m_ShowPhysicsColliders, "Display Box2D colliders");
		
		Separator();

		Text("Physics 3D");
		AddSpacing(SpacingValues::XS);
		PropertyCheckbox("Show 3D colliders", m_ShowPhysics3DColliders, "Display Bullet3D colliders");
	}
}
