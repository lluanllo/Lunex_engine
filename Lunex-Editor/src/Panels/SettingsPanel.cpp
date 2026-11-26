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
#include "Renderer/Deferred/DeferredRenderer.h"
#include "Rendering/RenderSystem.h"
#include "Renderer/Pipeline/RenderPipeline.h"
#include "Project/ProjectManager.h"
#include "Scene/Lighting/LightSystem.h"
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsConfig.h"
#include "Log/Log.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace Lunex {

	// ========================================
	// BLENDER-STYLE PROPERTIES LAYOUT
	// ========================================

	namespace {
		// Collapsible panel with a header bar and a boxed body (Blender properties editor).
		// The body box uses last frame's height so it can be drawn behind the content.
		struct PanelFrame { ImGuiID ID; ImVec2 Min; float Width; };
		std::vector<PanelFrame> s_PanelStack;

		ImU32 C32(const UI::Color& c) { return ImGui::GetColorU32(c.ToImVec4()); }

		enum class SettingsTab { Render = 0, World, Physics, Viewport, Count };

		// Small vector icons for the tab column (no image assets needed)
		void DrawTabIcon(ImDrawList* dl, SettingsTab tab, ImVec2 c, ImU32 col) {
			switch (tab) {
				case SettingsTab::Render:     // camera back
					dl->AddRect(ImVec2(c.x - 8, c.y - 5), ImVec2(c.x + 5, c.y + 6), col, 2.0f, 0, 1.6f);
					dl->AddTriangleFilled(ImVec2(c.x + 5, c.y), ImVec2(c.x + 10, c.y - 4), ImVec2(c.x + 10, c.y + 5), col);
					dl->AddCircleFilled(ImVec2(c.x - 1.5f, c.y + 0.5f), 2.2f, col);
					break;
				case SettingsTab::World:      // globe
					dl->AddCircle(c, 8.0f, col, 0, 1.6f);
					dl->AddEllipse(c, ImVec2(3.5f, 8.0f), col, 0.0f, 0, 1.3f);
					dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), col, 1.3f);
					break;
				case SettingsTab::Physics:    // orbit
					dl->AddCircle(c, 3.5f, col, 0, 1.6f);
					dl->AddEllipse(c, ImVec2(9.0f, 4.0f), col, -0.5f, 0, 1.3f);
					dl->AddCircleFilled(ImVec2(c.x + 7.0f, c.y - 3.5f), 2.0f, col);
					break;
				case SettingsTab::Viewport:   // grid
					for (int i = -1; i <= 1; i++) {
						dl->AddLine(ImVec2(c.x - 8, c.y + i * 5.0f), ImVec2(c.x + 8, c.y + i * 5.0f), col, 1.3f);
						dl->AddLine(ImVec2(c.x + i * 5.0f, c.y - 8), ImVec2(c.x + i * 5.0f, c.y + 8), col, 1.3f);
					}
					break;
				default: break;
			}
		}
	}

	bool SettingsPanel::BeginSettingsPanel(const char* title, bool defaultOpen) {
		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID id = ImGui::GetID(title);
		bool open = storage->GetBool(id, defaultOpen);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float w = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
		const float h = ImGui::GetFrameHeight() + 4.0f;

		// Body box first (height measured last frame), so the content draws on top
		if (open) {
			const float bodyH = storage->GetFloat(id + 1, 0.0f);
			if (bodyH > 0.0f)
				dl->AddRectFilled(ImVec2(p.x, p.y + h - 4.0f), ImVec2(p.x + w, p.y + h + bodyH), C32(UI::Colors::BgCard()), 5.0f, ImDrawFlags_RoundCornersBottom);
		}

		ImGui::PushID(title);
		const bool clicked = ImGui::InvisibleButton("##hdr", ImVec2(w, h));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (clicked) { open = !open; storage->SetBool(id, open); }

		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), C32(hovered ? UI::Colors::BgHover() : UI::Colors::BgLight()), 5.0f,
			open ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
		const float ax = p.x + 12.0f, ay = p.y + h * 0.5f;
		const ImU32 arrowCol = C32(UI::Colors::TextSecondary());
		if (open) dl->AddTriangleFilled(ImVec2(ax - 4, ay - 2), ImVec2(ax + 4, ay - 2), ImVec2(ax, ay + 3), arrowCol);
		else dl->AddTriangleFilled(ImVec2(ax - 2, ay - 4), ImVec2(ax + 3, ay), ImVec2(ax - 2, ay + 4), arrowCol);
		dl->AddText(ImVec2(p.x + 24.0f, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), C32(UI::Colors::TextPrimary()), title);

		if (!open) {
			ImGui::Dummy(ImVec2(0, 2));
			return false;
		}
		s_PanelStack.push_back({ id, ImVec2(p.x, p.y + h), w });
		ImGui::Indent(10.0f);
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::PushItemWidth(-10.0f);
		return true;
	}

	void SettingsPanel::EndSettingsPanel() {
		if (s_PanelStack.empty()) return;
		const PanelFrame frame = s_PanelStack.back();
		s_PanelStack.pop_back();
		ImGui::PopItemWidth();
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::Unindent(10.0f);
		ImGui::GetStateStorage()->SetFloat(frame.ID + 1, ImGui::GetCursorScreenPos().y - frame.Min.y);
		ImGui::Dummy(ImVec2(0, 4));
	}

	void SettingsPanel::OnImGuiRender() {
		using namespace UI;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		const bool visible = BeginPanel("Settings");
		ImGui::PopStyleVar();
		if (!visible) {
			EndPanel();
			return;
		}

		static const char* tabNames[] = { "Render", "World", "Physics", "Viewport" };
		static const char* tabTips[] = { "Render: engine, quality, effects, shadows, color management",
			"World: sky, HDRI and sun", "Physics: gravity and simulation", "Viewport: editor grid and overlays" };

		// ---- Vertical tab column (Blender properties editor) ----
		const float stripW = 34.0f;
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::BgDark().ToImVec4());
		ImGui::BeginChild("##SettingsTabs", ImVec2(stripW, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImGui::Dummy(ImVec2(0, 4));
		for (int t = 0; t < static_cast<int>(SettingsTab::Count); t++) {
			ImGui::SetCursorPosX(3.0f);
			const ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::PushID(t);
			if (ImGui::InvisibleButton("##tab", ImVec2(28, 28))) m_ActiveTab = t;
			const bool hovered = ImGui::IsItemHovered();
			ImGui::PopID();
			if (hovered) ImGui::SetTooltip("%s", tabTips[t]);
			const bool active = m_ActiveTab == t;
			if (active || hovered)
				dl->AddRectFilled(p, ImVec2(p.x + 28, p.y + 28), C32(active ? Colors::Primary().WithAlpha(0.85f) : Colors::BgHover()), 5.0f);
			DrawTabIcon(dl, static_cast<SettingsTab>(t), ImVec2(p.x + 14, p.y + 14), C32(active ? Color(1, 1, 1, 1) : Colors::TextSecondary()));
			ImGui::Dummy(ImVec2(0, 2));
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::SameLine(0, 0);

		// ---- Category content ----
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		ImGui::BeginChild("##SettingsContent", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
		SetPropertySplitLayout(true);
		ImGui::TextColored(Colors::TextSecondary().ToImVec4(), "%s", tabNames[m_ActiveTab]);
		ImGui::Dummy(ImVec2(0, 2));
		switch (static_cast<SettingsTab>(m_ActiveTab)) {
			case SettingsTab::Render:
				DrawRenderSection();
				DrawPostProcessSection();
				DrawShadowsSection();
				break;
			case SettingsTab::World:
				DrawEnvironmentSection();
				break;
			case SettingsTab::Physics:
				DrawPhysicsTab();
				break;
			case SettingsTab::Viewport:
				DrawViewportSection();
				break;
			default: break;
		}
		SetPropertySplitLayout(false);
		ImGui::EndChild();
		ImGui::PopStyleVar();

		EndPanel();
	}

	void SettingsPanel::DrawRenderTab() { DrawRenderSection(); }
	void SettingsPanel::DrawEnvironmentTab() { DrawEnvironmentSection(); }
	void SettingsPanel::DrawShadowsTab() { DrawShadowsSection(); }

	// ========================================
	// RENDER (engine, quality, AO, performance)
	// ========================================

	void SettingsPanel::DrawRenderSection() {
		using namespace UI;
		auto& config = RenderSystem::GetConfig();

		// Render engine sits above the panels, like Blender
		{
			const auto& pipelines = RenderPipelineRegistry::GetAll();
			std::vector<std::string> names;
			int current = 0;
			for (size_t i = 0; i < pipelines.size(); i++) {
				names.push_back(std::string(pipelines[i]->GetName()) + (pipelines[i]->IsAvailable() ? "" : "  (planned)"));
				if (pipelines[i]->GetType() == RenderPipelineRegistry::GetRequested()) current = static_cast<int>(i);
			}
			if (PropertyDropdown("Render Engine", current, names, "Which renderer draws the viewport and the game")) {
				RenderEngineType type = pipelines[current]->GetType();
				RenderPipelineRegistry::SetRequested(type);
				if (auto project = ProjectManager::GetActiveProject()) {
					project->GetConfig().RenderEngine = RenderEngineToString(type);
					if (m_OnPostProcessChanged) m_OnPostProcessChanged();   // saves the project config
				}
			}
			IRenderPipeline* requested = pipelines[current];
			if (!requested->IsAvailable())
				TextWrapped(("Rendering with " + std::string(RenderPipelineRegistry::GetActive()->GetName()) + " until it is implemented.").c_str(), TextVariant::Muted);
			AddSpacing(SpacingValues::SM);
		}

		if (BeginSettingsPanel("Quality")) {
			if (PropertyCheckbox("VSync", config.EnableVSync, "Enable vertical synchronization"))
				Application::Get().GetWindow().SetVSync(config.EnableVSync);
			PropertyCheckbox("HDR Rendering", config.EnableHDR, "Enable High Dynamic Range rendering");
			PropertyCheckbox("MSAA", config.EnableMSAA, "Enable Multi-Sample Anti-Aliasing");
			if (config.EnableMSAA) {
				int msaaIdx = 0;
				const char* msaaOptions[] = { "2x", "4x", "8x" };
				int msaaValues[] = { 2, 4, 8 };
				for (int i = 0; i < 3; i++) if (msaaValues[i] == (int)config.MSAASamples) { msaaIdx = i; break; }
				if (PropertyDropdown("Samples", msaaIdx, msaaOptions, 3, "Number of MSAA samples"))
					config.MSAASamples = msaaValues[msaaIdx];
			}
			EndSettingsPanel();
		}

		if (BeginSettingsPanel("Ambient Occlusion", false)) {
			PropertyCheckbox("Enable SSAO", config.EnableSSAO, "Screen-Space Ambient Occlusion");
			EndSettingsPanel();
		}

		if (BeginSettingsPanel("Performance", false)) {
			PropertyCheckbox("Parallel Draw Collection", config.EnableParallelDrawCollection,
				"Compute mesh world transforms on the job workers (scenes with 64+ meshes). GL submission stays on the main thread.");
			bool culling = DeferredRenderer::IsFrustumCullingEnabled();
			if (PropertyCheckbox("Frustum Culling", culling, "Skip submeshes outside the camera view"))
				DeferredRenderer::SetFrustumCullingEnabled(culling);
			EndSettingsPanel();
		}
	}

	// ========================================
	// VIEWPORT (editor-only drawing)
	// ========================================

	void SettingsPanel::DrawViewportSection() {
		using namespace UI;
		if (BeginSettingsPanel("Grid")) {
			auto& gridSettings = GridRenderer::GetSettings();
			PropertyColor("Color", gridSettings.GridColor);
			PropertyFloat("Scale", gridSettings.GridScale, 0.1f, 0.1f, 100.0f, "Size of each grid cell in units");
			PropertyFloat("Extent", gridSettings.FadeDistance, 1.0f, 5.0f, 500.0f, "How far the grid extends");
			PropertyFloat("Minor Thickness", gridSettings.MinorLineThickness, 0.1f, 0.1f, 5.0f, "Minor grid line thickness");
			PropertyFloat("Major Thickness", gridSettings.MajorLineThickness, 0.1f, 0.1f, 5.0f, "Major grid line thickness (every 10 lines)");
			EndSettingsPanel();
		}
		if (BeginSettingsPanel("Physics Overlays")) {
			PropertyCheckbox("2D Colliders", m_ShowPhysicsColliders, "Display Box2D colliders");
			PropertyCheckbox("3D Colliders", m_ShowPhysics3DColliders, "Display Bullet colliders");
			EndSettingsPanel();
		}
	}

	// ========================================
	// POST-PROCESS SECTION
	// ========================================
	
	void SettingsPanel::DrawPostProcessSection() {
		using namespace UI;
		
		{
			auto& ppConfig = PostProcessRenderer::GetConfig();
			auto& renderConfig = RenderSystem::GetConfig();
			
			if (!PostProcessRenderer::IsInitialized()) {
				TextColored(Colors::Warning(), "Post-processing not initialized (requires Deferred Rendering)");
				return;
			}
			
			// ========================================
			// BLOOM
			// ========================================
			if (BeginSettingsPanel("Bloom")) {
			
			if (PropertyCheckbox("Enable Bloom", ppConfig.EnableBloom, "Enable bloom post-processing effect")) {
				renderConfig.EnableBloom = ppConfig.EnableBloom;
				NotifyPostProcessChanged();
			}
			
			if (ppConfig.EnableBloom) {
				if (PropertyFloat("Threshold", ppConfig.BloomThreshold, 0.05f, 0.0f, 5.0f,
					"Brightness threshold for bloom extraction (lower = more glow)")) {
					renderConfig.BloomThreshold = ppConfig.BloomThreshold;
					NotifyPostProcessChanged();
				}
				
				if (PropertyFloat("Intensity", ppConfig.BloomIntensity, 0.01f, 0.0f, 3.0f,
					"Bloom contribution to the final image")) {
					renderConfig.BloomIntensity = ppConfig.BloomIntensity;
					NotifyPostProcessChanged();
				}
				
				if (PropertyFloat("Radius", ppConfig.BloomRadius, 0.1f, 0.1f, 5.0f,
					"Blur kernel spread (higher = wider glow)")) {
					renderConfig.BloomRadius = ppConfig.BloomRadius;
					NotifyPostProcessChanged();
				}
				
				float mipLevelsF = static_cast<float>(ppConfig.BloomMipLevels);
				if (PropertyFloat("Quality (Mip Levels)", mipLevelsF, 1.0f, 1.0f, 8.0f,
					"Number of downsample passes (more = smoother bloom, slightly slower)")) {
					ppConfig.BloomMipLevels = static_cast<int>(mipLevelsF);
					NotifyPostProcessChanged();
				}
			}
			
			Separator();
			
			// ========================================
			// VIGNETTE
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Vignette")) {
			
			if (PropertyCheckbox("Enable Vignette", ppConfig.EnableVignette, "Enable vignette effect")) {
				renderConfig.EnableVignette = ppConfig.EnableVignette;
				NotifyPostProcessChanged();
			}
			
			if (ppConfig.EnableVignette) {
				if (PropertySlider("Intensity##Vignette", ppConfig.VignetteIntensity, 0.0f, 1.0f, "%.2f",
					"How much the edges darken")) {
					renderConfig.VignetteIntensity = ppConfig.VignetteIntensity;
					NotifyPostProcessChanged();
				}
				
				if (PropertySlider("Roundness", ppConfig.VignetteRoundness, 0.0f, 2.0f, "%.2f",
					"Shape roundness (1.0 = circular, lower = more rectangular)")) {
					renderConfig.VignetteRoundness = ppConfig.VignetteRoundness;
					NotifyPostProcessChanged();
				}
				
				if (PropertySlider("Smoothness", ppConfig.VignetteSmoothness, 0.01f, 1.0f, "%.2f",
					"Transition smoothness from center to edges")) {
					renderConfig.VignetteSmoothness = ppConfig.VignetteSmoothness;
					NotifyPostProcessChanged();
				}
			}
			
			Separator();
			
			// ========================================
			// CHROMATIC ABERRATION
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Chromatic Aberration")) {
			
			if (PropertyCheckbox("Enable Chromatic Aberration", ppConfig.EnableChromaticAberration,
				"Enable chromatic aberration effect")) {
				renderConfig.EnableChromaticAberration = ppConfig.EnableChromaticAberration;
				NotifyPostProcessChanged();
			}
			
			if (ppConfig.EnableChromaticAberration) {
				if (PropertyFloat("Intensity##ChromAb", ppConfig.ChromaticAberrationIntensity, 0.1f, 0.0f, 20.0f,
					"Amount of color channel separation")) {
					renderConfig.ChromaticAberrationIntensity = ppConfig.ChromaticAberrationIntensity;
					NotifyPostProcessChanged();
				}
			}
			
			Separator();
			
			// ========================================
			// SCREEN-SPACE REFLECTIONS
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Screen-Space Reflections")) {

			if (PropertyCheckbox("Enable SSR", ppConfig.EnableSSR, "Enable screen-space reflections")) {
				NotifyPostProcessChanged();
			}

			if (ppConfig.EnableSSR) {
				if (PropertySlider("Intensity##SSR", ppConfig.SSRIntensity, 0.0f, 2.0f, "%.2f",
					"Strength of the reflections")) {
					NotifyPostProcessChanged();
				}
				if (PropertyFloat("Max Distance##SSR", ppConfig.SSRMaxDistance, 0.5f, 1.0f, 200.0f,
					"Maximum ray length in world units")) {
					NotifyPostProcessChanged();
				}
				if (PropertySlider("Thickness##SSR", ppConfig.SSRThickness, 0.01f, 5.0f, "%.2f",
					"Depth tolerance for a ray hit (higher = fewer gaps, more false hits)")) {
					NotifyPostProcessChanged();
				}
				float steps = static_cast<float>(ppConfig.SSRMaxSteps);
				if (PropertySlider("Steps##SSR", steps, 8.0f, 256.0f, "%.0f",
					"Ray march steps (quality vs. performance)")) {
					ppConfig.SSRMaxSteps = static_cast<int>(steps);
					NotifyPostProcessChanged();
				}
				if (PropertySlider("Max Roughness##SSR", ppConfig.SSRMaxRoughness, 0.05f, 1.0f, "%.2f",
					"Surfaces rougher than this receive no SSR")) {
					NotifyPostProcessChanged();
				}
			}

			Separator();

			// ========================================
			// TONE MAPPING
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Color Management")) {
			
			const char* toneMapOptions[] = { "ACES Film", "Reinhard", "Uncharted 2", "None (Linear)" };
			if (PropertyDropdown("Operator", ppConfig.ToneMapOperator, toneMapOptions, 4,
				"Tone mapping algorithm")) {
				renderConfig.ToneMapOperator = ppConfig.ToneMapOperator;
				NotifyPostProcessChanged();
			}
			
			if (PropertyFloat("Exposure##PP", ppConfig.Exposure, 0.01f, 0.01f, 10.0f,
				"Exposure multiplier before tone mapping")) {
				renderConfig.Exposure = ppConfig.Exposure;
				NotifyPostProcessChanged();
			}
			
			if (PropertyFloat("Gamma", ppConfig.Gamma, 0.01f, 1.0f, 3.0f,
				"Gamma correction value (2.2 = standard sRGB)")) {
				NotifyPostProcessChanged();
			}
			EndSettingsPanel();
			}
		}
	}
	
	void SettingsPanel::DrawEnvironmentSection() {
		using namespace UI;
		
		{
			if (BeginSettingsPanel("Environment", true)) {
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
				
				Button("Drop HDRI Here (.hdr, .exr)", ButtonVariant::Default, ButtonSize::Large, Size(-1, 40));
				
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Sun Light Synchronization")) {
			
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
			if (SkyboxRenderer::HasEnvironmentLoaded()) {
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
				
				if (PropertyColor("Background Color", bgColor)) {
					SkyboxRenderer::SetBackgroundColor(bgColor);
					SkyboxRenderer::ApplyBackgroundClearColor();
				}
			}
			EndSettingsPanel();
			}
		}
	}
	
	void SettingsPanel::DrawShadowsSection() {
		using namespace UI;
		
		{
			auto& shadowSystem = ShadowSystem::Get();
			ShadowConfig config = shadowSystem.GetConfig();
			bool configChanged = false;
			const char* resOptions[] = { "512", "1024", "2048", "4096" };
			int resValues[] = { 512, 1024, 2048, 4096 };
			if (BeginSettingsPanel("Shadows", true)) {
			
			// ========================================
			// GLOBAL SHADOW SETTINGS
			// ========================================
			bool shadowsEnabled = shadowSystem.IsEnabled();
			if (PropertyCheckbox("Enable Shadows", shadowsEnabled)) {
				shadowSystem.SetEnabled(shadowsEnabled);
			}
			
			if (!shadowsEnabled) {
				TextWrapped("Shadows are disabled globally.", TextVariant::Muted);
				EndSettingsPanel();
				return;
			}
			
			Separator();
			
			// ========================================
			// DIRECTIONAL / SUN LIGHT SHADOWS (CSM)
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Directional Light (CSM)")) {
			
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
			int resIdx = 2; // default 2048
			for (int i = 0; i < 4; i++) {
				if (resValues[i] == dirRes) { resIdx = i; break; }
			}
			if (PropertyDropdown("Resolution", resIdx, resOptions, 4, "Shadow map resolution per cascade")) {
				config.DirectionalResolution = resValues[resIdx];
				configChanged = true;
			}
			
			// Cascade count
			const char* cascadeOptions[] = { "1", "2", "3", "4", "5", "6" };
			int cascadeIdx = static_cast<int>(config.CSMCascadeCount) - 1;
			cascadeIdx = glm::clamp(cascadeIdx, 0, 5);
			if (PropertyDropdown("Cascade Count", cascadeIdx, cascadeOptions, 6, "Number of shadow cascades (1-6). Far cascades are cheap with the update interval below")) {
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Spot & Point Light Shadows")) {
			
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Filtering & Softening")) {
			
			// Filter mode (hard shadows for e.g. space games)
			const char* filterOptions[] = { "Hard", "Soft" };
			if (PropertyDropdown("Filter", config.FilterMode, filterOptions, 2,
				"Hard: crisp edges (single filtered tap). Soft: penumbra of constant world size")) {
				configChanged = true;
			}
			if (config.FilterMode == 1) {
				if (PropertySlider("Softness", config.Softness, 0.005f, 0.5f, "%.3f",
					"Penumbra radius in world units (same on every cascade)")) {
					configChanged = true;
				}
			}

			float interval = static_cast<float>(config.FarCascadeUpdateInterval);
			if (PropertySlider("Far Cascade Update", interval, 1.0f, 8.0f, "every %.0f frames",
				"Cascades 3+ are re-rendered every N frames (performance)")) {
				config.FarCascadeUpdateInterval = static_cast<uint32_t>(interval);
				configChanged = true;
			}

			// PCF Radius (spot / point lights)
			if (PropertyFloat("PCF Radius", config.PCFRadius, 0.1f, 0.0f, 8.0f, "Filter radius (texels) for spot and point light shadows")) {
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Shadow Color Tinting")) {
			
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Shadow Statistics")) {
			
			auto stats = shadowSystem.GetStatistics();
			TextColored(Colors::TextSecondary(), "  Maps Rendered: %d", stats.ShadowMapsRendered);
			TextColored(Colors::TextSecondary(), "  Cascades: %d", stats.CascadesRendered);
			TextColored(Colors::TextSecondary(), "  Spot Maps: %d", stats.SpotMapsRendered);
			TextColored(Colors::TextSecondary(), "  Point Faces: %d", stats.PointFacesRendered);
			TextColored(Colors::TextSecondary(), "  Draw Calls: %d", stats.ShadowDrawCalls);
			EndSettingsPanel();
			}

			// Apply config changes
			if (configChanged) {
				shadowSystem.SetConfig(config);
			}
		}
	}
	
	void SettingsPanel::DrawPhysicsTab() {
		DrawPhysicsGeneralSection();
		
		DrawPhysics2DSection();
		
		DrawPhysics3DSection();
	}
	
	void SettingsPanel::DrawPhysicsGeneralSection() {
		using namespace UI;
		
		{
			auto& physicsCore = PhysicsCore::Get();
			PhysicsConfig config = physicsCore.GetConfig();
			bool configChanged = false;
			if (BeginSettingsPanel("World Gravity")) {
			
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
			SameLine();
			if (Button("Moon", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, -1.62f, 0.0f);
				configChanged = true;
			}
			SameLine();
			if (Button("Mars", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, -3.72f, 0.0f);
				configChanged = true;
			}
			SameLine();
			if (Button("Zero-G", ButtonVariant::Outline)) {
				config.Gravity = glm::vec3(0.0f, 0.0f, 0.0f);
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// SIMULATION SETTINGS
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Simulation")) {
			
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Continuous Collision Detection")) {
			
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
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Debug")) {
			
			bool debugDraw = config.EnableDebugDraw;
			if (PropertyCheckbox("Debug Draw", debugDraw, "Render physics debug wireframes")) {
				config.EnableDebugDraw = debugDraw;
				configChanged = true;
			}
			
			Separator();
			
			// ========================================
			// STATISTICS
			// ========================================
				EndSettingsPanel();
			}
			if (BeginSettingsPanel("Statistics")) {
			
			auto* world = physicsCore.GetWorld();
			if (world) {
				TextColored(Colors::TextSecondary(), "  Rigid Bodies: %d", world->GetNumRigidBodies());
				TextColored(Colors::TextSecondary(), "  Contact Manifolds: %d", world->GetNumManifolds());
				TextColored(Colors::TextSecondary(), "  Sim Steps (total): %d", physicsCore.GetSimulationSteps());
			} else {
				TextColored(Colors::TextMuted(), "  Physics not initialized");
			}
			EndSettingsPanel();
			}

			if (configChanged) {
				physicsCore.SetConfig(config);
			}
		}
	}
	
	void SettingsPanel::DrawPhysics2DSection() {
		using namespace UI;
		
		{
			if (BeginSettingsPanel("Physics 2D", false)) {
			if (PropertyCheckbox("Show 2D colliders", m_ShowPhysicsColliders, "Display Box2D colliders")) {
				// Value updated by reference
			}
			EndSettingsPanel();
			}
		}
	}
	
	void SettingsPanel::DrawPhysics3DSection() {
		using namespace UI;
		
		{
			if (BeginSettingsPanel("Physics 3D", false)) {
			if (PropertyCheckbox("Show 3D colliders", m_ShowPhysics3DColliders, "Display Bullet3D colliders")) {
				// Value updated by reference
			}
			EndSettingsPanel();
			}
		}
	}
	
	// ========================================
	// POST-PROCESS CONFIG LOAD / SAVE
	// ========================================
	
	void SettingsPanel::LoadPostProcessFromConfig(const PostProcessPreferencesConfig& config) {
		auto& ppConfig = PostProcessRenderer::GetConfig();
		auto& renderConfig = RenderSystem::GetConfig();
		
		ppConfig.EnableBloom = config.EnableBloom;
		ppConfig.BloomThreshold = config.BloomThreshold;
		ppConfig.BloomIntensity = config.BloomIntensity;
		ppConfig.BloomRadius = config.BloomRadius;
		ppConfig.BloomMipLevels = config.BloomMipLevels;
		
		ppConfig.EnableVignette = config.EnableVignette;
		ppConfig.VignetteIntensity = config.VignetteIntensity;
		ppConfig.VignetteRoundness = config.VignetteRoundness;
		ppConfig.VignetteSmoothness = config.VignetteSmoothness;
		
		ppConfig.EnableChromaticAberration = config.EnableChromaticAberration;
		ppConfig.ChromaticAberrationIntensity = config.ChromaticAberrationIntensity;
		
		ppConfig.ToneMapOperator = config.ToneMapOperator;
		ppConfig.Exposure = config.Exposure;
		ppConfig.Gamma = config.Gamma;

		ppConfig.EnableSSR = config.EnableSSR;
		ppConfig.SSRIntensity = config.SSRIntensity;
		ppConfig.SSRMaxDistance = config.SSRMaxDistance;
		ppConfig.SSRThickness = config.SSRThickness;
		ppConfig.SSRMaxSteps = config.SSRMaxSteps;
		ppConfig.SSRMaxRoughness = config.SSRMaxRoughness;

		// Sync RenderSystem config
		renderConfig.EnableBloom = config.EnableBloom;
		renderConfig.BloomThreshold = config.BloomThreshold;
		renderConfig.BloomIntensity = config.BloomIntensity;
		renderConfig.BloomRadius = config.BloomRadius;
		renderConfig.EnableVignette = config.EnableVignette;
		renderConfig.VignetteIntensity = config.VignetteIntensity;
		renderConfig.VignetteRoundness = config.VignetteRoundness;
		renderConfig.VignetteSmoothness = config.VignetteSmoothness;
		renderConfig.EnableChromaticAberration = config.EnableChromaticAberration;
		renderConfig.ChromaticAberrationIntensity = config.ChromaticAberrationIntensity;
		renderConfig.ToneMapOperator = config.ToneMapOperator;
		renderConfig.Exposure = config.Exposure;
		
		LNX_LOG_INFO("Post-processing preferences loaded from project config");
	}
	
	void SettingsPanel::SavePostProcessToConfig(PostProcessPreferencesConfig& config) const {
		const auto& ppConfig = PostProcessRenderer::GetConfig();
		
		config.EnableBloom = ppConfig.EnableBloom;
		config.BloomThreshold = ppConfig.BloomThreshold;
		config.BloomIntensity = ppConfig.BloomIntensity;
		config.BloomRadius = ppConfig.BloomRadius;
		config.BloomMipLevels = ppConfig.BloomMipLevels;
		
		config.EnableVignette = ppConfig.EnableVignette;
		config.VignetteIntensity = ppConfig.VignetteIntensity;
		config.VignetteRoundness = ppConfig.VignetteRoundness;
		config.VignetteSmoothness = ppConfig.VignetteSmoothness;
		
		config.EnableChromaticAberration = ppConfig.EnableChromaticAberration;
		config.ChromaticAberrationIntensity = ppConfig.ChromaticAberrationIntensity;
		
		config.ToneMapOperator = ppConfig.ToneMapOperator;
		config.Exposure = ppConfig.Exposure;
		config.Gamma = ppConfig.Gamma;

		config.EnableSSR = ppConfig.EnableSSR;
		config.SSRIntensity = ppConfig.SSRIntensity;
		config.SSRMaxDistance = ppConfig.SSRMaxDistance;
		config.SSRThickness = ppConfig.SSRThickness;
		config.SSRMaxSteps = ppConfig.SSRMaxSteps;
		config.SSRMaxRoughness = ppConfig.SSRMaxRoughness;
	}
	
	void SettingsPanel::NotifyPostProcessChanged() {
		if (m_OnPostProcessChanged)
			m_OnPostProcessChanged();
	}
}
