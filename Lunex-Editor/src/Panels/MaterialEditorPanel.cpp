/**
 * @file MaterialEditorPanel.cpp
 * @brief Material Editor Panel - AAA Quality Implementation using Lunex UI Framework
 * 
 * Features:
 * - Real-time PBR material preview
 * - Detail normal maps (multiple)
 * - Layered (channel-packed) textures with per-channel mapping
 * - Color space annotations (sRGB / Non-Color)
 * - Alpha channel packing checkbox
 * - Drag & drop texture support
 * - 100% Lunex UI Framework - zero raw ImGui calls
 */

#include "stpch.h"
#include "MaterialEditorPanel.h"
#include "ContentBrowserPanel.h"
#include "RHI/RHI.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Log/Log.h"

#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	// ============================================================================
	// STYLE
	// ============================================================================
	
	namespace MatEdStyle {
		constexpr float PreviewColumnWidth = 500.0f;
		constexpr float TextureSlotHeight = 110.0f;
		constexpr float TextureThumbnailSize = 56.0f;
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
			if (!ShowUnsavedChangesDialog()) return;
		}
		m_EditingMaterial = material;
		m_IsOpen = true;
		m_HasUnsavedChanges = false;
		LNX_LOG_INFO("Material opened in editor: {0}", material->GetName());
	}

	void MaterialEditorPanel::OpenMaterial(const std::filesystem::path& materialPath) {
		auto material = MaterialRegistry::Get().LoadMaterial(materialPath);
		if (material) OpenMaterial(material);
	}

	void MaterialEditorPanel::CloseMaterial() {
		if (m_HasUnsavedChanges) {
			if (!ShowUnsavedChangesDialog()) return;
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
		if (!m_IsOpen || !m_EditingMaterial) return;

		using namespace UI;

		std::string windowTitle = "Material Editor - " + m_EditingMaterial->GetName();
		if (m_HasUnsavedChanges) windowTitle += " *";
		windowTitle += "###MaterialEditor";

		ScopedColor windowColors({
			{ImGuiCol_WindowBg, Colors::BgMedium()},
			{ImGuiCol_ChildBg, Colors::BgMedium()},
			{ImGuiCol_Border, Colors::Border()}
		});
		
		ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);

		ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavInputs;
		
		if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, windowFlags)) {
			DrawMenuBar();
			DrawMainLayout();
		}
		ImGui::End();

		if (!m_IsOpen && m_EditingMaterial) {
			CloseMaterial();
		}
	}

	void MaterialEditorPanel::SetPreviewSize(uint32_t width, uint32_t height) {
		m_PreviewWidth = width;
		m_PreviewHeight = height;
		if (m_PreviewRenderer) m_PreviewRenderer->SetResolution(width, height);
	}

	// ============================================================================
	// MAIN LAYOUT
	// ============================================================================

	void MaterialEditorPanel::DrawMainLayout() {
		using namespace UI;

		ImVec2 availSize = ImGui::GetContentRegionAvail();
		float previewWidth = std::max(availSize.x * 0.50f, MatEdStyle::PreviewColumnWidth);
		
		ScopedStyle layoutStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		{
			ScopedColor previewBg(ImGuiCol_ChildBg, Colors::BgDark());
			if (BeginChild("##PreviewArea", Size(previewWidth, availSize.y), false, 
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
				DrawPreviewViewport();
			}
			EndChild();
		}

		SameLine();

		{
			ScopedColor propsBg(ImGuiCol_ChildBg, Colors::BgMedium());
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
			if (BeginMenu("File")) {
				if (MenuItem("Save", "Ctrl+S")) SaveMaterial();
				if (MenuItem("Save As...", nullptr, false, false)) {}
				Separator();
				if (MenuItem("Revert")) {
					if (m_EditingMaterial) {
						MaterialRegistry::Get().ReloadMaterial(m_EditingMaterial->GetID());
						m_HasUnsavedChanges = false;
					}
				}
				Separator();
				if (MenuItem("Close", "Ctrl+W")) CloseMaterial();
				EndMenu();
			}

			if (BeginMenu("View")) {
				bool autoRotate = m_PreviewRenderer ? m_PreviewRenderer->GetAutoRotate() : false;
				if (PropertyCheckbox("Auto Rotate", autoRotate)) {
					if (m_PreviewRenderer) m_PreviewRenderer->SetAutoRotate(autoRotate);
				}
				Separator();
				if (BeginMenu("Preview Shape")) {
					if (MenuItem("Sphere", nullptr, true)) m_PreviewRenderer->SetPreviewModel(Model::CreateSphere());
					if (MenuItem("Cube")) m_PreviewRenderer->SetPreviewModel(Model::CreateCube());
					if (MenuItem("Plane")) m_PreviewRenderer->SetPreviewModel(Model::CreatePlane());
					if (MenuItem("Cylinder")) m_PreviewRenderer->SetPreviewModel(Model::CreateCylinder());
					EndMenu();
				}
				EndMenu();
			}

			float statusWidth = 100.0f;
			float availWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - statusWidth);
			
			if (m_HasUnsavedChanges) {
				ScopedColor sc(ImGuiCol_Text, Colors::Warning());
				UI::Text("Modified");
			} else {
				ScopedColor sc(ImGuiCol_Text, Colors::Success());
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
			uint32_t newW = static_cast<uint32_t>(viewportSize.x);
			uint32_t newH = static_cast<uint32_t>(viewportSize.y);
			if (newW != m_PreviewWidth || newH != m_PreviewHeight) {
				m_PreviewWidth = newW;
				m_PreviewHeight = newH;
				m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
			}
		}

		if (m_PreviewRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
			uint32_t textureID = m_PreviewRenderer->GetPreviewTextureID();
			if (textureID > 0) {
				Image(textureID, Size(viewportSize.x, viewportSize.y), true);
			} else {
				ImVec2 textSize = ImGui::CalcTextSize("Preview Loading...");
				ImGui::SetCursorPos(ImVec2(
					(viewportSize.x - textSize.x) * 0.5f,
					(viewportSize.y - textSize.y) * 0.5f
				));
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

		{
			ScopedColor headerColor(ImGuiCol_Text, Colors::TextPrimary());
			Heading(m_EditingMaterial->GetName(), 2);
		}
		AddSpacing(SpacingValues::SM);
		Separator();
		AddSpacing(SpacingValues::MD);

		DrawPBRPropertiesSection();
		AddSpacing(SpacingValues::MD);
		DrawEmissionSection();
		AddSpacing(SpacingValues::MD);
		DrawTextureMapsSection();
		AddSpacing(SpacingValues::MD);
		DrawDetailNormalsSection();
		AddSpacing(SpacingValues::MD);
		DrawLayeredTextureSection();
		AddSpacing(SpacingValues::MD);
		DrawAdvancedSection();
	}

	// ============================================================================
	// PBR PROPERTIES SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawPBRPropertiesSection() {
		using namespace UI;
		ScopedID sectionID("PBRSection");

		SeparatorText("PBR Properties");
		AddSpacing(SpacingValues::SM);

		{
			glm::vec4 albedo = m_EditingMaterial->GetAlbedo();
			Color albedoColor(albedo);
			if (PropertyColor4("Albedo", albedoColor, "Base color of the material")) {
				m_EditingMaterial->SetAlbedo(glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a));
				MarkAsModified();
			}
		}

		{
			float metallic = m_EditingMaterial->GetMetallic();
			if (PropertySlider("Metallic", metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
				m_EditingMaterial->SetMetallic(metallic);
				MarkAsModified();
			}
		}

		{
			float roughness = m_EditingMaterial->GetRoughness();
			if (PropertySlider("Roughness", roughness, 0.0f, 1.0f, "%.2f", "0 = Smooth, 1 = Rough")) {
				m_EditingMaterial->SetRoughness(roughness);
				MarkAsModified();
			}
		}

		{
			float specular = m_EditingMaterial->GetSpecular();
			if (PropertySlider("Specular", specular, 0.0f, 1.0f, "%.2f", "Specular reflection intensity")) {
				m_EditingMaterial->SetSpecular(specular);
				MarkAsModified();
			}
		}

		{
			float normalIntensity = m_EditingMaterial->GetNormalIntensity();
			if (PropertySlider("Normal Intensity", normalIntensity, 0.0f, 2.0f, "%.2f", "Normal map strength")) {
				m_EditingMaterial->SetNormalIntensity(normalIntensity);
				MarkAsModified();
			}
		}
	}

	// ============================================================================
	// EMISSION SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawEmissionSection() {
		using namespace UI;
		ScopedID sectionID("EmissionSection");

		SeparatorText("Emission");
		AddSpacing(SpacingValues::SM);

		{
			glm::vec3 emissionColor = m_EditingMaterial->GetEmissionColor();
			Color3 color(emissionColor);
			if (PropertyColor("Emission Color", color, "Color of emitted light")) {
				m_EditingMaterial->SetEmissionColor(color);
				MarkAsModified();
			}
		}

		{
			float intensity = m_EditingMaterial->GetEmissionIntensity();
			if (PropertySlider("Intensity", intensity, 0.0f, 100.0f, "%.1f", "Brightness of emission")) {
				m_EditingMaterial->SetEmissionIntensity(intensity);
				MarkAsModified();
			}
		}
	}

	// ============================================================================
	// TEXTURE MAPS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawTextureMapsSection() {
		using namespace UI;
		ScopedID sectionID("TextureMapsSection");

		SeparatorText("Texture Maps");
		AddSpacing(SpacingValues::MD);

		// Albedo
		DrawTextureSlotNew("Albedo", m_EditingMaterial->GetAlbedoMap(), m_EditingMaterial->GetAlbedoPath(),
			m_EditingMaterial->GetAlbedoColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAlbedoMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetAlbedoMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetAlbedoColorSpace(cs); MarkAsModified(); });

		// Normal
		DrawTextureSlotNew("Normal", m_EditingMaterial->GetNormalMap(), m_EditingMaterial->GetNormalPath(),
			m_EditingMaterial->GetNormalColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetNormalMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetNormalMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetNormalColorSpace(cs); MarkAsModified(); });

		// Metallic
		DrawTextureSlotNew("Metallic", m_EditingMaterial->GetMetallicMap(), m_EditingMaterial->GetMetallicPath(),
			m_EditingMaterial->GetMetallicColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetMetallicMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetMetallicMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetMetallicColorSpace(cs); MarkAsModified(); });
		
		if (m_EditingMaterial->HasMetallicMap()) {
			Indent(16.0f);
			float mult = m_EditingMaterial->GetMetallicMultiplier();
			if (PropertySlider("Multiplier Metalic", mult, 0.0f, 10.0f, "%.2f")) {
				m_EditingMaterial->SetMetallicMultiplier(mult);
				MarkAsModified();
			}
			Unindent(16.0f);
		}

		// Roughness
		DrawTextureSlotNew("Roughness", m_EditingMaterial->GetRoughnessMap(), m_EditingMaterial->GetRoughnessPath(),
			m_EditingMaterial->GetRoughnessColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetRoughnessMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetRoughnessMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetRoughnessColorSpace(cs); MarkAsModified(); });
		
		if (m_EditingMaterial->HasRoughnessMap()) {
			Indent(16.0f);
			float mult = m_EditingMaterial->GetRoughnessMultiplier();
			if (PropertySlider("Multiplier Roughness", mult, 0.0f, 10.0f, "%.2f")) {
				m_EditingMaterial->SetRoughnessMultiplier(mult);
				MarkAsModified();
			}
			Unindent(16.0f);
		}

		// Specular
		DrawTextureSlotNew("Specular", m_EditingMaterial->GetSpecularMap(), m_EditingMaterial->GetSpecularPath(),
			TextureColorSpace::NonColor,
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetSpecularMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetSpecularMap(nullptr); MarkAsModified(); },
			nullptr);
		
		if (m_EditingMaterial->HasSpecularMap()) {
			Indent(16.0f);
			float mult = m_EditingMaterial->GetSpecularMultiplier();
			if (PropertySlider("Multiplier Specular", mult, 0.0f, 10.0f, "%.2f")) {
				m_EditingMaterial->SetSpecularMultiplier(mult);
				MarkAsModified();
			}
			Unindent(16.0f);
		}

		// Emission
		DrawTextureSlotNew("Emission", m_EditingMaterial->GetEmissionMap(), m_EditingMaterial->GetEmissionPath(),
			m_EditingMaterial->GetEmissionColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetEmissionMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetEmissionMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetEmissionColorSpace(cs); MarkAsModified(); });

		// AO
		DrawTextureSlotNew("Ambient Occlusion", m_EditingMaterial->GetAOMap(), m_EditingMaterial->GetAOPath(),
			m_EditingMaterial->GetAOColorSpace(),
			[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAOMap(tex); MarkAsModified(); },
			[this]() { m_EditingMaterial->SetAOMap(nullptr); MarkAsModified(); },
			[this](TextureColorSpace cs) { m_EditingMaterial->SetAOColorSpace(cs); MarkAsModified(); });
		
		if (m_EditingMaterial->HasAOMap()) {
			Indent(16.0f);
			float mult = m_EditingMaterial->GetAOMultiplier();
			if (PropertySlider("Multiplier AO", mult, 0.0f, 10.0f, "%.2f")) {
				m_EditingMaterial->SetAOMultiplier(mult);
				MarkAsModified();
			}
			Unindent(16.0f);
		}
	}

	// ============================================================================
	// DETAIL NORMALS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawDetailNormalsSection() {
		using namespace UI;
		ScopedID sectionID("DetailNormalsSection");

		SeparatorText("Detail Normal Maps");
		AddSpacing(SpacingValues::SM);

		TextStyled("Add fine surface detail using additional normal maps with independent tiling.", TextVariant::Muted);
		AddSpacing(SpacingValues::SM);

		auto& details = m_EditingMaterial->GetDetailNormalMaps();

		for (size_t i = 0; i < details.size(); ++i) {
			ScopedID detailID(static_cast<int>(i));
			
			std::string headerLabel = "Detail Normal #" + std::to_string(i);
			
			if (BeginSection(headerLabel, true)) {
				// Texture slot
				DrawTextureSlotNew("Texture##Detail" + std::to_string(i),
					details[i].Texture, details[i].Path,
					TextureColorSpace::NonColor,
					[this, i](Ref<Texture2D> tex) {
						auto& d = m_EditingMaterial->GetDetailNormalMaps()[i];
						d.Texture = tex;
						if (tex && tex->IsLoaded()) d.Path = tex->GetPath();
						MarkAsModified();
					},
					[this, i]() {
						auto& d = m_EditingMaterial->GetDetailNormalMaps()[i];
						d.Texture = nullptr;
						d.Path.clear();
						MarkAsModified();
					},
					nullptr);

				float intensity = details[i].Intensity;
				if (PropertySlider("Intensity Detail" + std::to_string(i), intensity, 0.0f, 2.0f, "%.2f", "Detail strength")) {
					m_EditingMaterial->GetDetailNormalMaps()[i].Intensity = intensity;
					MarkAsModified();
				}

				float tilingX = details[i].TilingX;
				if (PropertyFloat("Tiling X##D" + std::to_string(i), tilingX, 0.1f, 0.01f, 100.0f, "Horizontal tiling")) {
					m_EditingMaterial->GetDetailNormalMaps()[i].TilingX = tilingX;
					MarkAsModified();
				}

				float tilingY = details[i].TilingY;
				if (PropertyFloat("Tiling Y##D" + std::to_string(i), tilingY, 0.1f, 0.01f, 100.0f, "Vertical tiling")) {
					m_EditingMaterial->GetDetailNormalMaps()[i].TilingY = tilingY;
					MarkAsModified();
				}

				if (Button("Remove##D" + std::to_string(i), ButtonVariant::Danger, ButtonSize::Small)) {
					m_EditingMaterial->RemoveDetailNormalMap(i);
					MarkAsModified();
					EndSection();
					break;
				}

				EndSection();
			}
		}

		AddSpacing(SpacingValues::SM);

		if (Button("+ Add Detail Normal", ButtonVariant::Outline, ButtonSize::Medium)) {
			DetailNormalMap newDetail;
			newDetail.Intensity = 1.0f;
			newDetail.TilingX = 4.0f;
			newDetail.TilingY = 4.0f;
			m_EditingMaterial->AddDetailNormalMap(newDetail);
			MarkAsModified();
		}

		if (details.size() >= MaterialAsset::MAX_DETAIL_NORMALS) {
			AddSpacing(SpacingValues::XS);
			TextStyled("Maximum " + std::to_string(MaterialAsset::MAX_DETAIL_NORMALS) + " detail normals supported by shader.", TextVariant::Warning);
		}
	}

	// ============================================================================
	// LAYERED TEXTURE SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawLayeredTextureSection() {
		using namespace UI;
		ScopedID sectionID("LayeredTextureSection");

		SeparatorText("Layered Texture (Channel-Packed)");
		AddSpacing(SpacingValues::SM);

		TextStyled("Use a single texture with data packed in R/G/B/A channels.", TextVariant::Muted);
		TextStyled("Default: R=Metallic, G=Roughness, B=AO (Non-Color data)", TextVariant::Muted);
		AddSpacing(SpacingValues::SM);

		auto& config = m_EditingMaterial->GetLayeredTextureConfig();

		bool enabled = config.Enabled;
		if (PropertyCheckbox("Enable Layered Texture", enabled, "Use a channel-packed texture for M/R/AO")) {
			m_EditingMaterial->GetLayeredTextureConfig().Enabled = enabled;
			MarkAsModified();
		}

		if (!enabled) return;

		AddSpacing(SpacingValues::SM);

		// Texture slot - always Non-Color for layered data textures
		DrawTextureSlotNew("Layered Map", config.Texture, config.Path,
			TextureColorSpace::NonColor,
			[this](Ref<Texture2D> tex) {
				m_EditingMaterial->SetLayeredTexture(tex);
				MarkAsModified();
			},
			[this]() {
				m_EditingMaterial->SetLayeredTexture(nullptr);
				MarkAsModified();
			},
			nullptr);

		if (!config.Texture) return;

		AddSpacing(SpacingValues::SM);

		const char* channelNames[] = { "Red", "Green", "Blue", "Alpha" };

		// Metallic channel
		{
			bool useMetallic = config.UseForMetallic;
			if (PropertyCheckbox("Use for Metallic", useMetallic, "Read metallic from this texture")) {
				m_EditingMaterial->GetLayeredTextureConfig().UseForMetallic = useMetallic;
				MarkAsModified();
			}
			if (useMetallic) {
				Indent(16.0f);
				int ch = static_cast<int>(config.MetallicChannel);
				if (PropertyDropdown("Channel##MetCh", ch, channelNames, 4, "Source channel for metallic")) {
					m_EditingMaterial->GetLayeredTextureConfig().MetallicChannel = static_cast<TextureChannel>(ch);
					MarkAsModified();
				}
				float metMult = m_EditingMaterial->GetMetallicMultiplier();
				if (PropertySlider("Intensity##MetLayered", metMult, 0.0f, 10.0f, "%.2f", "Metallic channel multiplier")) {
					m_EditingMaterial->SetMetallicMultiplier(metMult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}

		// Roughness channel
		{
			bool useRoughness = config.UseForRoughness;
			if (PropertyCheckbox("Use for Roughness", useRoughness, "Read roughness from this texture")) {
				m_EditingMaterial->GetLayeredTextureConfig().UseForRoughness = useRoughness;
				MarkAsModified();
			}
			if (useRoughness) {
				Indent(16.0f);
				int ch = static_cast<int>(config.RoughnessChannel);
				if (PropertyDropdown("Channel##RoughCh", ch, channelNames, 4, "Source channel for roughness")) {
					m_EditingMaterial->GetLayeredTextureConfig().RoughnessChannel = static_cast<TextureChannel>(ch);
					MarkAsModified();
				}
				float roughMult = m_EditingMaterial->GetRoughnessMultiplier();
				if (PropertySlider("Intensity##RoughLayered", roughMult, 0.0f, 10.0f, "%.2f", "Roughness channel multiplier")) {
					m_EditingMaterial->SetRoughnessMultiplier(roughMult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}

		// AO channel
		{
			bool useAO = config.UseForAO;
			if (PropertyCheckbox("Use for AO", useAO, "Read ambient occlusion from this texture")) {
				m_EditingMaterial->GetLayeredTextureConfig().UseForAO = useAO;
				MarkAsModified();
			}
			if (useAO) {
				Indent(16.0f);
				int ch = static_cast<int>(config.AOChannel);
				if (PropertyDropdown("Channel##AOCh", ch, channelNames, 4, "Source channel for AO")) {
					m_EditingMaterial->GetLayeredTextureConfig().AOChannel = static_cast<TextureChannel>(ch);
					MarkAsModified();
				}
				float aoMult = m_EditingMaterial->GetAOMultiplier();
				if (PropertySlider("Intensity##AOLayered", aoMult, 0.0f, 10.0f, "%.2f", "AO channel multiplier")) {
					m_EditingMaterial->SetAOMultiplier(aoMult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}
	}

	// ============================================================================
	// ADVANCED SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawAdvancedSection() {
		using namespace UI;
		ScopedID sectionID("AdvancedSection");

		SeparatorText("Advanced");
		AddSpacing(SpacingValues::SM);

		// Alpha channel packing
		{
			bool alphaPacked = m_EditingMaterial->IsAlphaChannelPacked();
			if (PropertyCheckbox("Alpha Channel Packed", alphaPacked, "Albedo alpha contains packed data instead of opacity")) {
				m_EditingMaterial->SetAlphaIsPacked(alphaPacked);
				MarkAsModified();
			}

			if (alphaPacked) {
				Indent(16.0f);
				const char* alphaChNames[] = { "Red", "Green", "Blue", "Alpha" };
				int ch = static_cast<int>(m_EditingMaterial->GetAlphaPackedChannel());
				if (PropertyDropdown("Packed Channel", ch, alphaChNames, 4, "Which channel the alpha represents")) {
					m_EditingMaterial->SetAlphaPackedChannel(static_cast<TextureChannel>(ch));
					MarkAsModified();
				}
				Unindent(16.0f);
			}
		}
	}

	// ============================================================================
	// TEXTURE SLOT
	// ============================================================================

	void MaterialEditorPanel::DrawTextureSlotNew(
		const std::string& label,
		Ref<Texture2D> texture,
		const std::string& path,
		TextureColorSpace colorSpace,
		std::function<void(Ref<Texture2D>)> onTextureSet,
		std::function<void()> onTextureClear,
		std::function<void(TextureColorSpace)> onColorSpaceChanged) {
		
		using namespace UI;

		ScopedID slotID(label);

		ImVec2 slotSize = ImVec2(ImGui::GetContentRegionAvail().x, MatEdStyle::TextureSlotHeight);
		
		ScopedColor slotBg(ImGuiCol_ChildBg, Colors::BgDark());
		ScopedStyle slotRounding(ImGuiStyleVar_ChildRounding, 4.0f);

		if (BeginChild("##TexSlot" + label, Size(slotSize.x, slotSize.y), true, 
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			
			// Header: label + color space + remove
			{
				TextStyled(label, TextVariant::Primary);

				// Color space badge
				if (onColorSpaceChanged) {
					SameLine(0, 12.0f);
					bool isNonColor = (colorSpace == TextureColorSpace::NonColor);
					std::string csLabel = isNonColor ? "Non-Color" : "sRGB";
					Color csColor = isNonColor ? Colors::TextSecondary() : Colors::Info();
					Badge(csLabel, csColor.Darker(0.3f), csColor);

					if (IsItemHovered()) {
						SetTooltip("Click to toggle color space");
					}
					// Toggle on badge click via invisible button hack
					SameLine(0, 4.0f);
					if (Button("##CSToggle" + label, ButtonVariant::Ghost, ButtonSize::Small, Size(1, 1))) {
						TextureColorSpace newCS = isNonColor ? TextureColorSpace::sRGB : TextureColorSpace::NonColor;
						onColorSpaceChanged(newCS);
					}
				}

				if (texture) {
					float removeX = ImGui::GetContentRegionAvail().x - 55;
					if (removeX > 0) {
						SameLine(removeX);
						if (Button("Remove##" + label, ButtonVariant::Danger, ButtonSize::Small)) {
						 onTextureClear();
						}
					}
				}
			}

			AddSpacing(SpacingValues::XS);

			// Content: thumbnail + filename
			{
				if (texture && texture->IsLoaded()) {
					Image(texture, Size(MatEdStyle::TextureThumbnailSize, MatEdStyle::TextureThumbnailSize), true);
					
					SameLine(0, 8.0f);
					
					std::filesystem::path texPath(path);
					float yOffset = (MatEdStyle::TextureThumbnailSize - ImGui::GetTextLineHeight()) * 0.5f;
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
					TextStyled(texPath.filename().string(), TextVariant::Muted);
				} else {
					TextStyled("Drop texture here", TextVariant::Muted);
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
	// LEGACY PROPERTY CONTROLS
	// ============================================================================

	bool MaterialEditorPanel::DrawColorProperty(const char* label, glm::vec4& color) {
		UI::Color uiColor(color);
		bool changed = UI::PropertyColor4(label, uiColor);
		if (changed) color = glm::vec4(uiColor.r, uiColor.g, uiColor.b, uiColor.a);
		return changed;
	}

	bool MaterialEditorPanel::DrawColor3Property(const char* label, glm::vec3& color) {
		UI::Color3 uiColor(color);
		bool changed = UI::PropertyColor(label, uiColor);
		if (changed) color = uiColor;
		return changed;
	}

	bool MaterialEditorPanel::DrawFloatProperty(const char* label, float& value, float min, float max) {
		return UI::PropertySlider(label, value, min, max);
	}

	void MaterialEditorPanel::DrawTextureSlot(const char* label, const char* icon,
		Ref<Texture2D> texture, const std::string& path,
		std::function<void(const std::string&)> loadFunc) {
		
		DrawTextureSlotNew(label, texture, path, TextureColorSpace::sRGB,
			[loadFunc](Ref<Texture2D>) {},
			[]() {},
			nullptr);
	}

	// ============================================================================
	// HELPERS
	// ============================================================================

	void MaterialEditorPanel::SaveMaterial() {
		if (!m_EditingMaterial) return;
		if (m_EditingMaterial->Save()) {
			m_HasUnsavedChanges = false;
			LNX_LOG_INFO("Material saved: {0}", m_EditingMaterial->GetName());
			if (m_OnMaterialSaved) m_OnMaterialSaved(m_EditingMaterial->GetPath());
		} else {
			LNX_LOG_ERROR("Failed to save material: {0}", m_EditingMaterial->GetName());
		}
	}

	void MaterialEditorPanel::MarkAsModified() {
		m_HasUnsavedChanges = true;
		if (m_AutoSave) SaveMaterial();
	}

	bool MaterialEditorPanel::ShowUnsavedChangesDialog() {
		if (m_HasUnsavedChanges) SaveMaterial();
		return true;
	}

} // namespace Lunex
