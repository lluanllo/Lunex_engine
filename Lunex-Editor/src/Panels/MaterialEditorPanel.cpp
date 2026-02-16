/**
 * @file MaterialEditorPanel.cpp
 * @brief Material Editor Panel - AAA Quality Implementation using Lunex UI Framework
 * 
 * Features:
 * - Real-time PBR material preview
 * - Drag & drop texture support
 * - Layered (ORM) packed texture support (R=Metallic, G=Roughness, B=AO)
 * - Height/Parallax mapping
 * - Detail normal maps with independent UV tiling
 * - Surface settings (alpha mode, two-sided, UV tiling/offset)
 * - Material statistics and info panel
 * - Clean, professional UI with no unnecessary scrollbars
 * - Proper ImGui ID handling to prevent window movement bugs
 */

#include "stpch.h"
#include "MaterialEditorPanel.h"
#include "ContentBrowserPanel.h"
#include "RHI/RHI.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	// ============================================================================
	// PANEL STYLE CONSTANTS
	// ============================================================================
	
	namespace MaterialEditorStyle {
		// Layout
		constexpr float PreviewColumnWidth = 500.0f;
		constexpr float PropertiesMinWidth = 350.0f;
		constexpr float PropertyLabelWidth = 130.0f;
		constexpr float TextureSlotHeight = 130.0f;
		constexpr float TextureThumbnailSize = 64.0f;
		
		// Colors
		UI::Color BgPanel()         { return UI::Color(0.12f, 0.12f, 0.13f, 1.0f); }
		UI::Color BgPreview()       { return UI::Color(0.08f, 0.08f, 0.09f, 1.0f); }
		UI::Color BgSection()       { return UI::Color(0.14f, 0.14f, 0.15f, 1.0f); }
		UI::Color BgTextureSlot()   { return UI::Color(0.11f, 0.11f, 0.12f, 1.0f); }
		UI::Color BorderSubtle()    { return UI::Color(0.20f, 0.20f, 0.22f, 1.0f); }
		UI::Color AccentPrimary()   { return UI::Color(0.26f, 0.59f, 0.98f, 1.0f); }
		UI::Color AccentLayered()   { return UI::Color(0.90f, 0.55f, 0.15f, 1.0f); }
		UI::Color StatusSaved()     { return UI::Color(0.30f, 0.80f, 0.30f, 1.0f); }
		UI::Color StatusUnsaved()   { return UI::Color(0.95f, 0.75f, 0.20f, 1.0f); }
		UI::Color ButtonDanger()    { return UI::Color(0.70f, 0.25f, 0.25f, 1.0f); }
		UI::Color ButtonDangerHover() { return UI::Color(0.80f, 0.30f, 0.30f, 1.0f); }
		UI::Color TextMuted()       { return UI::Color(0.50f, 0.50f, 0.55f, 1.0f); }
		UI::Color TextHeader()      { return UI::Color(0.90f, 0.90f, 0.92f, 1.0f); }
		UI::Color ChannelR()        { return UI::Color(0.85f, 0.25f, 0.25f, 1.0f); }
		UI::Color ChannelG()        { return UI::Color(0.25f, 0.80f, 0.25f, 1.0f); }
		UI::Color ChannelB()        { return UI::Color(0.25f, 0.40f, 0.90f, 1.0f); }
		UI::Color InfoBg()          { return UI::Color(0.10f, 0.12f, 0.15f, 1.0f); }
	}

	// ============================================================================
	// CONSTRUCTOR
	// ============================================================================
	
	MaterialEditorPanel::MaterialEditorPanel() {
		m_PreviewRenderer = CreateRef<MaterialPreviewRenderer>();
		m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
	}

	// ============================================================================
	// PANEL CONTROL
	// ============================================================================

	void MaterialEditorPanel::OpenMaterial(Ref<MaterialAsset> material) {
		if (!material) {
			LNX_LOG_ERROR("MaterialEditorPanel::OpenMaterial - Material is null");
			return;
		}

		if (m_EditingMaterial && m_HasUnsavedChanges) {
			if (!ShowUnsavedChangesDialog()) {
				return;
			}
		}

		m_EditingMaterial = material;
		m_IsOpen = true;
		m_HasUnsavedChanges = false;

		LNX_LOG_INFO("Material opened in editor: {0}", material->GetName());
	}

	void MaterialEditorPanel::OpenMaterial(const std::filesystem::path& materialPath) {
		auto material = MaterialRegistry::Get().LoadMaterial(materialPath);
		if (material) {
			OpenMaterial(material);
		}
	}

	void MaterialEditorPanel::CloseMaterial() {
		if (m_HasUnsavedChanges) {
			if (!ShowUnsavedChangesDialog()) {
				return;
			}
		}

		m_EditingMaterial.reset();
		m_IsOpen = false;
		m_HasUnsavedChanges = false;
	}

	// ============================================================================
	// UPDATE & RENDER
	// ============================================================================

	void MaterialEditorPanel::OnUpdate(float deltaTime) {
		if (m_PreviewRenderer && m_EditingMaterial) {
			m_PreviewRenderer->Update(deltaTime);
			m_PreviewRenderer->RenderPreview(m_EditingMaterial);
		}
	}

	void MaterialEditorPanel::OnImGuiRender() {
		if (!m_IsOpen || !m_EditingMaterial) {
			return;
		}

		using namespace UI;

		// Window title with unsaved indicator
		std::string windowTitle = "Material Editor - " + m_EditingMaterial->GetName();
		if (m_HasUnsavedChanges) {
			windowTitle += " *";
		}
		windowTitle += "###MaterialEditor";

		// Window styling
		ScopedColor windowColors({
			{ImGuiCol_WindowBg, MaterialEditorStyle::BgPanel()},
			{ImGuiCol_ChildBg, MaterialEditorStyle::BgPanel()},
			{ImGuiCol_Border, MaterialEditorStyle::BorderSubtle()}
		});
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

		ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavInputs;
		
		if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, windowFlags)) {
			ImGui::PopStyleVar(2);
			DrawMenuBar();
			DrawMainLayout();
		} else {
			ImGui::PopStyleVar(2);
		}
		ImGui::End();

		// Handle window close
		if (!m_IsOpen && m_EditingMaterial) {
			CloseMaterial();
		}
	}

	void MaterialEditorPanel::SetPreviewSize(uint32_t width, uint32_t height) {
		m_PreviewWidth = width;
		m_PreviewHeight = height;
		if (m_PreviewRenderer) {
			m_PreviewRenderer->SetResolution(width, height);
		}
	}

	// ============================================================================
	// MAIN LAYOUT
	// ============================================================================

	void MaterialEditorPanel::DrawMainLayout() {
		using namespace UI;

		ImVec2 availSize = ImGui::GetContentRegionAvail();
		float previewWidth = std::max(availSize.x * 0.55f, MaterialEditorStyle::PreviewColumnWidth);
		
		ScopedStyle layoutStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// Left: Preview viewport
		{
			ScopedColor previewBg(ImGuiCol_ChildBg, MaterialEditorStyle::BgPreview());
			
			if (BeginChild("##PreviewArea", Size(previewWidth, availSize.y), false, 
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
				DrawPreviewViewport();
			}
			EndChild();
		}

		SameLine();

		// Right: Properties panel (with vertical scroll enabled for texture maps)
		{
			ScopedColor propsBg(ImGuiCol_ChildBg, MaterialEditorStyle::BgPanel());
			ScopedStyle propsPadding(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
			
			if (BeginChild("##PropertiesArea", Size(0, availSize.y), false, ImGuiWindowFlags_None)) {
				DrawPropertiesPanel();
			}
			EndChild();
		}
	}

	// ============================================================================
	// MENU BAR
	// ============================================================================

	void MaterialEditorPanel::DrawMenuBar() {
		using namespace UI;

		if (BeginMenuBar()) {
			// File menu
			if (BeginMenu("File")) {
				if (MenuItem("Save", "Ctrl+S")) {
					SaveMaterial();
				}
				if (MenuItem("Save As...", nullptr, false, false)) {
					// TODO: Save As dialog
				}
				Separator();
				if (MenuItem("Revert")) {
					if (m_EditingMaterial) {
						MaterialRegistry::Get().ReloadMaterial(m_EditingMaterial->GetID());
						m_HasUnsavedChanges = false;
					}
				}
				Separator();
				if (MenuItem("Close", "Ctrl+W")) {
					CloseMaterial();
				}
				EndMenu();
			}

			// View menu
			if (BeginMenu("View")) {
				bool autoRotate = m_PreviewRenderer ? m_PreviewRenderer->GetAutoRotate() : false;
				if (ImGui::Checkbox("Auto Rotate Preview", &autoRotate)) {
					if (m_PreviewRenderer) {
						m_PreviewRenderer->SetAutoRotate(autoRotate);
					}
				}
				
				Separator();
				
				if (BeginMenu("Preview Shape")) {
					if (MenuItem("Sphere", nullptr, true)) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateSphere());
					}
					if (MenuItem("Cube")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateCube());
					}
					if (MenuItem("Plane")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreatePlane());
					}
					if (MenuItem("Cylinder")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateCylinder());
					}
					EndMenu();
				}
				EndMenu();
			}

			// Status indicator (right-aligned)
			float statusWidth = 100.0f;
			float availWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - statusWidth);
			
			if (m_HasUnsavedChanges) {
				ScopedColor statusColor(ImGuiCol_Text, MaterialEditorStyle::StatusUnsaved());
				UI::Text("Modified");
			} else {
				ScopedColor statusColor(ImGuiCol_Text, MaterialEditorStyle::StatusSaved());
				UI::Text("Saved");
			}

			EndMenuBar();
		}
	}

	// ============================================================================
	// PREVIEW VIEWPORT
	// ============================================================================

	void MaterialEditorPanel::DrawPreviewViewport() {
		using namespace UI;

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		if (m_PreviewRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
			uint32_t newWidth = static_cast<uint32_t>(viewportSize.x);
			uint32_t newHeight = static_cast<uint32_t>(viewportSize.y);
			
			if (newWidth != m_PreviewWidth || newHeight != m_PreviewHeight) {
				m_PreviewWidth = newWidth;
				m_PreviewHeight = newHeight;
				m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
			}
		}

		if (m_PreviewRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
			uint32_t textureID = m_PreviewRenderer->GetPreviewTextureID();
			if (textureID > 0) {
				UI::Image(textureID, Size(viewportSize.x, viewportSize.y), true);
			} else {
				ImVec2 textSize = ImGui::CalcTextSize("Preview Loading...");
				ImVec2 cursorPos = ImVec2(
					(viewportSize.x - textSize.x) * 0.5f,
					(viewportSize.y - textSize.y) * 0.5f
				);
				ImGui::SetCursorPos(cursorPos);
				TextStyled("Preview Loading...", TextVariant::Muted);
			}
		}
	}

	// ============================================================================
	// PROPERTIES PANEL
	// ============================================================================

	void MaterialEditorPanel::DrawPropertiesPanel() {
		if (!m_EditingMaterial) return;

		using namespace UI;

		ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

		// Material name header
		{
			ScopedColor headerColor(ImGuiCol_Text, MaterialEditorStyle::TextHeader());
			UI::Text("%s", m_EditingMaterial->GetName().c_str());
		}
		AddSpacing(SpacingValues::SM);
		Separator();
		AddSpacing(SpacingValues::MD);

		// PBR Properties Section
		DrawPBRPropertiesSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Surface Settings Section
		DrawSurfaceSettingsSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Emission Section
		DrawEmissionSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Texture Maps Section
		DrawTextureMapsSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Layered (ORM) Texture Section
		DrawLayeredTextureSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Height Map / Parallax Section
		DrawHeightMapSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Detail Normal Map Section
		DrawDetailMapSection();
		
		AddSpacing(SpacingValues::MD);
		
		// Material Info Section
		DrawMaterialInfoSection();
		
		AddSpacing(SpacingValues::XL);
	}

	void MaterialEditorPanel::DrawPBRPropertiesSection() {
		using namespace UI;

		ScopedID sectionID("PBRSection");

		SeparatorText("PBR Properties");
		AddSpacing(SpacingValues::SM);

		// Albedo color
		{
			glm::vec4 albedo = m_EditingMaterial->GetAlbedo();
			UI::Color albedoColor(albedo);
			if (PropertyColor4("Albedo", albedoColor, "Base color of the material")) {
				m_EditingMaterial->SetAlbedo(glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a));
				MarkAsModified();
			}
		}

		// Metallic
		{
			float metallic = m_EditingMaterial->GetMetallic();
			if (PropertySlider("Metallic", metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
				m_EditingMaterial->SetMetallic(metallic);
				MarkAsModified();
			}
		}

		// Roughness
		{
			float roughness = m_EditingMaterial->GetRoughness();
			if (PropertySlider("Roughness", roughness, 0.0f, 1.0f, "%.2f", "0 = Smooth, 1 = Rough")) {
				m_EditingMaterial->SetRoughness(roughness);
				MarkAsModified();
			}
		}

		// Specular
		{
			float specular = m_EditingMaterial->GetSpecular();
			if (PropertySlider("Specular", specular, 0.0f, 1.0f, "%.2f", "Specular reflection intensity")) {
				m_EditingMaterial->SetSpecular(specular);
				MarkAsModified();
			}
		}

		// Normal Intensity
		{
			float normalIntensity = m_EditingMaterial->GetNormalIntensity();
			if (PropertySlider("Normal Intensity", normalIntensity, 0.0f, 2.0f, "%.2f", "Strength of normal map")) {
				m_EditingMaterial->SetNormalIntensity(normalIntensity);
				MarkAsModified();
			}
		}
	}

	// ============================================================================
	// SURFACE SETTINGS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawSurfaceSettingsSection() {
		using namespace UI;

		ScopedID sectionID("SurfaceSection");

		SeparatorText("Surface Settings");
		AddSpacing(SpacingValues::SM);

		// Alpha Mode dropdown
		{
			const char* alphaModes[] = { "Opaque", "Cutoff", "Transparent" };
			int currentMode = static_cast<int>(m_EditingMaterial->GetAlphaMode());
			if (PropertyDropdown("Alpha Mode", currentMode, alphaModes, 3, "Surface transparency mode")) {
				m_EditingMaterial->SetAlphaMode(static_cast<AlphaMode>(currentMode));
				MarkAsModified();
			}
		}

		// Alpha Cutoff (only visible in Cutoff mode)
		if (m_EditingMaterial->GetAlphaMode() == AlphaMode::Cutoff) {
			float cutoff = m_EditingMaterial->GetAlphaCutoff();
			if (PropertySlider("Alpha Cutoff", cutoff, 0.0f, 1.0f, "%.2f", "Pixels below this alpha are discarded")) {
				m_EditingMaterial->SetAlphaCutoff(cutoff);
				MarkAsModified();
			}
		}

		// Two-sided
		{
			bool twoSided = m_EditingMaterial->IsTwoSided();
			if (PropertyCheckbox("Two Sided", twoSided, "Render both faces of polygons")) {
				m_EditingMaterial->SetTwoSided(twoSided);
				MarkAsModified();
			}
		}

		// UV Tiling
		{
			glm::vec2 tiling = m_EditingMaterial->GetUVTiling();
			if (PropertyVec2("UV Tiling", tiling, 0.01f, "Texture repeat in U and V")) {
				m_EditingMaterial->SetUVTiling(tiling);
				MarkAsModified();
			}
		}

		// UV Offset
		{
			glm::vec2 offset = m_EditingMaterial->GetUVOffset();
			if (PropertyVec2("UV Offset", offset, 0.01f, "Texture offset in U and V")) {
				m_EditingMaterial->SetUVOffset(offset);
				MarkAsModified();
			}
		}
	}

	void MaterialEditorPanel::DrawEmissionSection() {
		using namespace UI;

		ScopedID sectionID("EmissionSection");

		SeparatorText("Emission");
		AddSpacing(SpacingValues::SM);

		// Emission color
		{
			glm::vec3 emissionColor = m_EditingMaterial->GetEmissionColor();
			UI::Color3 color(emissionColor);
			if (PropertyColor("Emission Color", color, "Color of emitted light")) {
				m_EditingMaterial->SetEmissionColor(color);
				MarkAsModified();
			}
		}

		// Emission intensity
		{
			float intensity = m_EditingMaterial->GetEmissionIntensity();
			if (PropertySlider("Intensity", intensity, 0.0f, 100.0f, "%.1f", "Brightness of emission")) {
				m_EditingMaterial->SetEmissionIntensity(intensity);
				MarkAsModified();
			}
		}
	}

	void MaterialEditorPanel::DrawTextureMapsSection() {
		using namespace UI;

		ScopedID sectionID("TextureMapsSection");

		SeparatorText("Texture Maps");
		AddSpacing(SpacingValues::MD);

		// Albedo Map
		DrawTextureSlotNew("Albedo", m_EditingMaterial->GetAlbedoMap(), m_EditingMaterial->GetAlbedoPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAlbedoMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetAlbedoMap(nullptr); MarkAsModified(); });

		// Normal Map
		DrawTextureSlotNew("Normal", m_EditingMaterial->GetNormalMap(), m_EditingMaterial->GetNormalPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetNormalMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetNormalMap(nullptr); MarkAsModified(); });

		// Metallic Map with multiplier (disabled when layered map is active)
		{
			bool layeredActive = m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap();
			ScopedDisabled disabled(layeredActive);
			
			DrawTextureSlotNew("Metallic", m_EditingMaterial->GetMetallicMap(), m_EditingMaterial->GetMetallicPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetMetallicMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetMetallicMap(nullptr); MarkAsModified(); });
			
			if (m_EditingMaterial->HasMetallicMap() && !layeredActive) {
				Indent(16.0f);
				float mult = m_EditingMaterial->GetMetallicMultiplier();
				if (PropertySlider("Multiplier Metallic", mult, 0.0f, 2.0f, "%.2f")) {
					m_EditingMaterial->SetMetallicMultiplier(mult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}

		// Roughness Map with multiplier (disabled when layered map is active)
		{
			bool layeredActive = m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap();
			ScopedDisabled disabled(layeredActive);

			DrawTextureSlotNew("Roughness", m_EditingMaterial->GetRoughnessMap(), m_EditingMaterial->GetRoughnessPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetRoughnessMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetRoughnessMap(nullptr); MarkAsModified(); });
			
			if (m_EditingMaterial->HasRoughnessMap() && !layeredActive) {
				Indent(16.0f);
				float mult = m_EditingMaterial->GetRoughnessMultiplier();
				if (PropertySlider("Multiplier Roughness", mult, 0.0f, 2.0f, "%.2f")) {
					m_EditingMaterial->SetRoughnessMultiplier(mult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}

		// Specular Map with multiplier
		DrawTextureSlotNew("Specular", m_EditingMaterial->GetSpecularMap(), m_EditingMaterial->GetSpecularPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetSpecularMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetSpecularMap(nullptr); MarkAsModified(); });
		
		if (m_EditingMaterial->HasSpecularMap()) {
			Indent(16.0f);
			float mult = m_EditingMaterial->GetSpecularMultiplier();
			if (PropertySlider("Multiplier Specular", mult, 0.0f, 2.0f, "%.2f")) {
				m_EditingMaterial->SetSpecularMultiplier(mult);
				MarkAsModified();
			}
			Unindent(16.0f);
		}

		// Emission Map
		DrawTextureSlotNew("Emission", m_EditingMaterial->GetEmissionMap(), m_EditingMaterial->GetEmissionPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetEmissionMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetEmissionMap(nullptr); MarkAsModified(); });

		// AO Map with multiplier (disabled when layered map is active)
		{
			bool layeredActive = m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap();
			ScopedDisabled disabled(layeredActive);

			DrawTextureSlotNew("Ambient Occlusion", m_EditingMaterial->GetAOMap(), m_EditingMaterial->GetAOPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAOMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetAOMap(nullptr); MarkAsModified(); });
			
			if (m_EditingMaterial->HasAOMap() && !layeredActive) {
				Indent(16.0f);
				float mult = m_EditingMaterial->GetAOMultiplier();
				if (PropertySlider("Multiplier AO", mult, 0.0f, 2.0f, "%.2f")) {
					m_EditingMaterial->SetAOMultiplier(mult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}
	}

	// ============================================================================
	// LAYERED (ORM) TEXTURE SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawLayeredTextureSection() {
		using namespace UI;

		ScopedID sectionID("LayeredTextureSection");

		SeparatorText("Layered Texture (ORM)");
		AddSpacing(SpacingValues::SM);

		// Info text
		TextWrapped("Use a single packed texture for Metallic, Roughness and AO. "
			"Standard format: R=Metallic, G=Roughness, B=AO. "
			"Reduces texture count and VRAM usage.", TextVariant::Muted);
		AddSpacing(SpacingValues::SM);

		// Enable layered map
		{
			bool useLayered = m_EditingMaterial->GetUseLayeredMap();
			if (PropertyCheckbox("Enable ORM", useLayered, "Use packed texture instead of separate maps")) {
				m_EditingMaterial->SetUseLayeredMap(useLayered);
				MarkAsModified();
			}
		}

		if (m_EditingMaterial->GetUseLayeredMap()) {
			AddSpacing(SpacingValues::SM);

			// Layered texture slot
			DrawTextureSlotNew("ORM Texture", m_EditingMaterial->GetLayeredMap(), m_EditingMaterial->GetLayeredPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetLayeredMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetLayeredMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasLayeredMap()) {
				AddSpacing(SpacingValues::SM);
				
				// Channel assignment
				const char* channelNames[] = { "Red (R)", "Green (G)", "Blue (B)" };

				{
					int chMetallic = m_EditingMaterial->GetLayeredChannelMetallic();
					ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelR());
					if (PropertyDropdown("Metallic Channel", chMetallic, channelNames, 3, "Which channel contains metallic data")) {
						m_EditingMaterial->SetLayeredChannelMetallic(chMetallic);
						MarkAsModified();
					}
				}

				{
					int chRoughness = m_EditingMaterial->GetLayeredChannelRoughness();
					ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelG());
					if (PropertyDropdown("Roughness Channel", chRoughness, channelNames, 3, "Which channel contains roughness data")) {
						m_EditingMaterial->SetLayeredChannelRoughness(chRoughness);
						MarkAsModified();
					}
				}

				{
					int chAO = m_EditingMaterial->GetLayeredChannelAO();
					ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelB());
					if (PropertyDropdown("AO Channel", chAO, channelNames, 3, "Which channel contains ambient occlusion data")) {
						m_EditingMaterial->SetLayeredChannelAO(chAO);
						MarkAsModified();
					}
				}

				// Multipliers still apply
				AddSpacing(SpacingValues::SM);
				{
					float metallicMult = m_EditingMaterial->GetMetallicMultiplier();
					if (PropertySlider("Metallic Mult##ORM", metallicMult, 0.0f, 2.0f, "%.2f", "Multiplier for metallic from ORM")) {
						m_EditingMaterial->SetMetallicMultiplier(metallicMult);
						MarkAsModified();
					}
				}
				{
					float roughnessMult = m_EditingMaterial->GetRoughnessMultiplier();
					if (PropertySlider("Roughness Mult##ORM", roughnessMult, 0.0f, 2.0f, "%.2f", "Multiplier for roughness from ORM")) {
						m_EditingMaterial->SetRoughnessMultiplier(roughnessMult);
						MarkAsModified();
					}
				}
				{
					float aoMult = m_EditingMaterial->GetAOMultiplier();
					if (PropertySlider("AO Mult##ORM", aoMult, 0.0f, 2.0f, "%.2f", "Multiplier for AO from ORM")) {
						m_EditingMaterial->SetAOMultiplier(aoMult);
						MarkAsModified();
					}
				}

				// Warn about separate maps being overridden
				if (m_EditingMaterial->HasMetallicMap() || m_EditingMaterial->HasRoughnessMap() || m_EditingMaterial->HasAOMap()) {
					AddSpacing(SpacingValues::SM);
					TextWrapped("Note: ORM texture overrides separate Metallic, Roughness and AO maps.", TextVariant::Warning);
				}
			}
		}
	}

	// ============================================================================
	// HEIGHT MAP / PARALLAX SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawHeightMapSection() {
		using namespace UI;

		ScopedID sectionID("HeightMapSection");

		SeparatorText("Height / Parallax");
		AddSpacing(SpacingValues::SM);

		DrawTextureSlotNew("Height Map", m_EditingMaterial->GetHeightMap(), m_EditingMaterial->GetHeightPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetHeightMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetHeightMap(nullptr); MarkAsModified(); });

		if (m_EditingMaterial->HasHeightMap()) {
			Indent(16.0f);
			
			float heightScale = m_EditingMaterial->GetHeightScale();
			if (PropertySlider("Height Scale", heightScale, 0.0f, 0.5f, "%.3f", "Depth of parallax displacement")) {
				m_EditingMaterial->SetHeightScale(heightScale);
				MarkAsModified();
			}

			Unindent(16.0f);
		}
	}

	// ============================================================================
	// DETAIL NORMAL MAP SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawDetailMapSection() {
		using namespace UI;

		ScopedID sectionID("DetailMapSection");

		SeparatorText("Detail Normal");
		AddSpacing(SpacingValues::SM);

		TextWrapped("Adds high-frequency surface detail on top of the main normal map. "
			"Uses independent UV tiling for micro-detail.", TextVariant::Muted);
		AddSpacing(SpacingValues::SM);

		DrawTextureSlotNew("Detail Normal", m_EditingMaterial->GetDetailNormalMap(), m_EditingMaterial->GetDetailNormalPath(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetDetailNormalMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetDetailNormalMap(nullptr); MarkAsModified(); });

		if (m_EditingMaterial->HasDetailNormalMap()) {
			Indent(16.0f);
			
			float detailScale = m_EditingMaterial->GetDetailNormalScale();
			if (PropertySlider("Detail Intensity", detailScale, 0.0f, 2.0f, "%.2f", "Strength of detail normal")) {
				m_EditingMaterial->SetDetailNormalScale(detailScale);
				MarkAsModified();
			}

			glm::vec2 detailTiling = m_EditingMaterial->GetDetailUVTiling();
			if (PropertyVec2("Detail UV Tiling", detailTiling, 0.1f, "Independent UV tiling for detail texture")) {
				m_EditingMaterial->SetDetailUVTiling(detailTiling);
				MarkAsModified();
			}

			Unindent(16.0f);
		}
	}

	// ============================================================================
	// MATERIAL INFO SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawMaterialInfoSection() {
		using namespace UI;

		ScopedID sectionID("MaterialInfoSection");

		SeparatorText("Material Info");
		AddSpacing(SpacingValues::SM);

		ScopedColor bgColor(ImGuiCol_ChildBg, MaterialEditorStyle::InfoBg());
		ScopedStyle rounding(ImGuiStyleVar_ChildRounding, 4.0f);

		if (BeginChild("##InfoPanel", Size(ImGui::GetContentRegionAvail().x, 0), true,
			ImGuiWindowFlags_NoScrollbar)) {

			// Asset ID
			TextStyled("Asset ID:", TextVariant::Muted);
			SameLine();
			UI::Text("%llu", (unsigned long long)(uint64_t)m_EditingMaterial->GetID());

			// File path
			if (!m_EditingMaterial->GetPath().empty()) {
				TextStyled("File:", TextVariant::Muted);
				SameLine();
				TextWrapped(m_EditingMaterial->GetPath().string());
			}

			// Texture count
			TextStyled("Textures:", TextVariant::Muted);
			SameLine();
			int texCount = m_EditingMaterial->GetTextureCount();
			UI::Text("%d loaded", texCount);

			// Alpha mode
			TextStyled("Alpha Mode:", TextVariant::Muted);
			SameLine();
			UI::Text("%s", AlphaModeToString(m_EditingMaterial->GetAlphaMode()));

			// ORM status
			if (m_EditingMaterial->GetUseLayeredMap()) {
				TextColored(MaterialEditorStyle::AccentLayered(), "ORM: Active");
				if (m_EditingMaterial->HasLayeredMap()) {
					SameLine();
					TextStyled("(saves 2 texture slots)", TextVariant::Muted);
				}
			}

			// Two-sided
			if (m_EditingMaterial->IsTwoSided()) {
				TextColored(MaterialEditorStyle::StatusUnsaved(), "Two-Sided: Active");
			}

			// Detail normal
			if (m_EditingMaterial->HasDetailNormalMap()) {
				TextColored(MaterialEditorStyle::AccentPrimary(), "Detail Normal: Active (%.1fx%.1f tiling)",
					m_EditingMaterial->GetDetailUVTiling().x,
					m_EditingMaterial->GetDetailUVTiling().y);
			}

			// Height map
			if (m_EditingMaterial->HasHeightMap()) {
				TextColored(MaterialEditorStyle::AccentPrimary(), "Parallax: Active (scale: %.3f)", m_EditingMaterial->GetHeightScale());
			}
		}
		EndChild();
	}

	// ============================================================================
	// TEXTURE SLOT RENDERING
	// ============================================================================

	void MaterialEditorPanel::DrawTextureSlotNew(
		const std::string& label,
		Ref<Texture2D> texture,
		const std::string& path,
		std::function<void(Ref<Texture2D>)> onTextureSet,
		std::function<void()> onTextureClear) {
		
		using namespace UI;

		ScopedID slotID(label);

		ImVec2 slotSize = ImVec2(ImGui::GetContentRegionAvail().x, MaterialEditorStyle::TextureSlotHeight);
		
		ScopedColor slotBg(ImGuiCol_ChildBg, MaterialEditorStyle::BgTextureSlot());
		ScopedStyle slotRounding(ImGuiStyleVar_ChildRounding, 4.0f);

		if (BeginChild("##TexSlot" + label, Size(slotSize.x, slotSize.y), true, 
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			
			// Header row: label + remove button
			{
				TextStyled(label, TextVariant::Primary);

				if (texture) {
					ImGui::SameLine(ImGui::GetContentRegionAvail().x - 55);
					
					ScopedColor btnColors({
						{ImGuiCol_Button, MaterialEditorStyle::ButtonDanger()},
						{ImGuiCol_ButtonHovered, MaterialEditorStyle::ButtonDangerHover()}
					});
					
					if (ImGui::SmallButton("Remove")) {
						onTextureClear();
					}
				}
			}

			AddSpacing(SpacingValues::XS);

			// Content row: thumbnail + filename
			{
				if (texture && texture->IsLoaded()) {
					// Thumbnail
					UI::Image(texture, Size(MaterialEditorStyle::TextureThumbnailSize, MaterialEditorStyle::TextureThumbnailSize));
					
					SameLine();
					
					// Filename + resolution info
					std::filesystem::path texPath(path);
					float yOffset = (MaterialEditorStyle::TextureThumbnailSize - ImGui::GetTextLineHeight() * 2) * 0.5f;
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
					ImGui::BeginGroup();
					TextStyled(texPath.filename().string(), TextVariant::Muted);
					TextColored(MaterialEditorStyle::TextMuted(), "%dx%d", texture->GetWidth(), texture->GetHeight());
					ImGui::EndGroup();
				} else {
					// Drop zone placeholder
					TextStyled("Drop texture here or click to browse", TextVariant::Muted);
				}
			}

			// Drag & drop target
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
						auto newTexture = Texture2D::Create(data->FilePath);
						if (newTexture && newTexture->IsLoaded()) {
							onTextureSet(newTexture);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
		EndChild();

		AddSpacing(SpacingValues::XS);
	}

	// ============================================================================
	// LEGACY PROPERTY CONTROLS (kept for compatibility, redirect to UI framework)
	// ============================================================================

	bool MaterialEditorPanel::DrawColorProperty(const char* label, glm::vec4& color) {
		UI::Color uiColor(color);
		bool changed = UI::PropertyColor4(label, uiColor);
		if (changed) {
			color = glm::vec4(uiColor.r, uiColor.g, uiColor.b, uiColor.a);
		}
		return changed;
	}

	bool MaterialEditorPanel::DrawColor3Property(const char* label, glm::vec3& color) {
		UI::Color3 uiColor(color);
		bool changed = UI::PropertyColor(label, uiColor);
		if (changed) {
			color = uiColor;
		}
		return changed;
	}

	bool MaterialEditorPanel::DrawFloatProperty(const char* label, float& value, float min, float max) {
		return UI::PropertySlider(label, value, min, max);
	}

	void MaterialEditorPanel::DrawTextureSlot(const char* label, const char* icon,
		Ref<Texture2D> texture, const std::string& path,
		std::function<void(const std::string&)> loadFunc) {
		
		// Redirect to new implementation
		DrawTextureSlotNew(label, texture, path,
			[loadFunc](Ref<Texture2D>) {
				// This is handled by the new implementation directly
			},
			[]() {
				// Handled by new implementation
			});
	}

	// ============================================================================
	// HELPERS
	// ============================================================================

	void MaterialEditorPanel::SaveMaterial() {
		if (!m_EditingMaterial) return;

		if (m_EditingMaterial->Save()) {
			m_HasUnsavedChanges = false;
			LNX_LOG_INFO("Material saved: {0}", m_EditingMaterial->GetName());
			
			if (m_OnMaterialSaved) {
				m_OnMaterialSaved(m_EditingMaterial->GetPath());
			}
		} else {
			LNX_LOG_ERROR("Failed to save material: {0}", m_EditingMaterial->GetName());
		}
	}

	void MaterialEditorPanel::MarkAsModified() {
		m_HasUnsavedChanges = true;
		if (m_AutoSave) {
			SaveMaterial();
		}
	}

	bool MaterialEditorPanel::ShowUnsavedChangesDialog() {
		// TODO: Implement proper modal dialog
		// For now, auto-save
		if (m_HasUnsavedChanges) {
			SaveMaterial();
		}
		return true;
	}

} // namespace Lunex
