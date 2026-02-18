/**
 * @file MaterialEditorPanel.cpp
 * @brief Material Editor Panel - Uses Lunex UI Framework exclusively
 *
 * Design principles:
 * - Collapsible sections with clear visual hierarchy
 * - No duplicate controls - each property appears exactly once
 * - Logical grouping: Base Color > PBR Parameters > Textures > Advanced
 * - Compact texture slots that don't waste vertical space
 * - Clean separation between preview (left) and properties (right)
 * - Zero raw ImGui calls - everything through Lunex::UI
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
	// STYLE CONSTANTS
	// ============================================================================

	namespace MatStyle {
		static constexpr float PreviewMinWidth = 400.0f;

		inline UI::Color BgPanel()       { return UI::Color(0.12f, 0.12f, 0.14f, 1.0f); }
		inline UI::Color BgPreview()     { return UI::Color(0.07f, 0.07f, 0.08f, 1.0f); }
		inline UI::Color BgSection()     { return UI::Color(0.15f, 0.15f, 0.17f, 1.0f); }
		inline UI::Color AccentPrimary() { return UI::Color(0.26f, 0.59f, 0.98f, 1.0f); }
		inline UI::Color AccentLayered() { return UI::Color(0.90f, 0.55f, 0.15f, 1.0f); }
		inline UI::Color AccentEmission(){ return UI::Color(0.95f, 0.80f, 0.20f, 1.0f); }
		inline UI::Color AccentHeight()  { return UI::Color(0.55f, 0.75f, 0.35f, 1.0f); }
		inline UI::Color StatusSaved()   { return UI::Color(0.30f, 0.80f, 0.30f, 1.0f); }
		inline UI::Color StatusUnsaved() { return UI::Color(0.95f, 0.75f, 0.20f, 1.0f); }
		inline UI::Color InfoBg()        { return UI::Color(0.10f, 0.11f, 0.13f, 1.0f); }
		inline UI::Color BorderSubtle()  { return UI::Color(0.22f, 0.22f, 0.25f, 1.0f); }
		inline UI::Color ChannelR()      { return UI::Color(0.85f, 0.30f, 0.30f, 1.0f); }
		inline UI::Color ChannelG()      { return UI::Color(0.30f, 0.80f, 0.30f, 1.0f); }
		inline UI::Color ChannelB()      { return UI::Color(0.30f, 0.45f, 0.90f, 1.0f); }
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

		m_SectionLayered = material->GetUseLayeredMap();
		m_SectionEmission = material->GetEmissionIntensity() > 0.0f || material->HasEmissionMap();
		m_SectionHeight = material->HasHeightMap();
		m_SectionDetail = material->HasDetailNormalMap();

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

		std::string windowTitle = "Material Editor - " + m_EditingMaterial->GetName();
		if (m_HasUnsavedChanges) {
			windowTitle += " *";
		}
		windowTitle += "###MaterialEditor";

		ScopedColor windowColors({
			{ColorVar::WindowBg, MatStyle::BgPanel()},
			{ColorVar::ChildBg, MatStyle::BgPanel()},
			{ColorVar::Border, MatStyle::BorderSubtle()}
		});

		ScopedStyle windowPadding(StyleVar::WindowPadding, ToImVec2(Size(0, 0)));
		ScopedStyle windowRounding(StyleVar::WindowRounding, 4.0f);

		SetNextWindowSize(Size(1100, 750));

		if (BeginWindow(windowTitle, &m_IsOpen,
						 WindowFlags::MenuBar | WindowFlags::NoNavInputs))
		{
			DrawMenuBar();
			DrawMainLayout();
		}
		EndWindow();

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

		Size availSize = GetContentRegionAvail();
		float previewWidth = std::max(availSize.x * 0.45f, MatStyle::PreviewMinWidth);

		ScopedStyle layoutStyle(StyleVar::ItemSpacing, ToImVec2(Size(0, 0)));

		// Left: Preview
		{
			ScopedColor previewBg(ColorVar::ChildBg, MatStyle::BgPreview());
			if (BeginChild("##PreviewArea", Size(previewWidth, availSize.y), false,
				WindowFlags::NoScrollbar | WindowFlags::NoScrollWithMouse))
			{
				DrawPreviewViewport();
			}
			EndChild();
		}

		SameLine();

		// Right: Properties
		{
			ScopedColor propsBg(ColorVar::ChildBg, MatStyle::BgPanel());
			ScopedStyle propsPadding(StyleVar::WindowPadding, ToImVec2(Size(0, 0)));
			if (BeginChild("##PropertiesArea", Size(0, availSize.y))) {
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
				if (MenuItem("Save", "Ctrl+S")) {
					SaveMaterial();
				}
				if (MenuItem("Save As...", nullptr, false, false)) {}
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

			if (BeginMenu("View")) {
				bool autoRotate = m_PreviewRenderer ? m_PreviewRenderer->GetAutoRotate() : false;
				if (PropertyCheckbox("Auto Rotate", autoRotate)) {
					if (m_PreviewRenderer) m_PreviewRenderer->SetAutoRotate(autoRotate);
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
				Separator();
				if (BeginMenu("Sections")) {
					PropertyCheckbox("Base Color & Albedo", m_SectionBaseColor);
					PropertyCheckbox("PBR Parameters", m_SectionPBR);
					PropertyCheckbox("Surface Settings", m_SectionSurface);
					PropertyCheckbox("Texture Maps", m_SectionTextures);
					PropertyCheckbox("Layered (ORM)", m_SectionLayered);
					PropertyCheckbox("Emission", m_SectionEmission);
					PropertyCheckbox("Height / Parallax", m_SectionHeight);
					PropertyCheckbox("Detail Normal", m_SectionDetail);
					PropertyCheckbox("Material Info", m_SectionInfo);
					EndMenu();
				}
				EndMenu();
			}

			// Status indicator right-aligned
			float statusWidth = 100.0f;
			Size menuAvail = GetContentRegionAvail();
			Position menuCursor = GetCursorPos();
			SetCursorPosX(menuCursor.x + menuAvail.x - statusWidth);

			if (m_HasUnsavedChanges) {
				StatusBadge("Modified", MatStyle::StatusUnsaved());
			} else {
				StatusBadge("Saved", MatStyle::StatusSaved());
			}

			EndMenuBar();
		}
	}

	// ============================================================================
	// PREVIEW VIEWPORT
	// ============================================================================

	void MaterialEditorPanel::DrawPreviewViewport() {
		using namespace UI;

		Size viewportSize = GetContentRegionAvail();

		if (m_PreviewRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
			uint32_t newW = static_cast<uint32_t>(viewportSize.x);
			uint32_t newH = static_cast<uint32_t>(viewportSize.y);

			if (newW != m_PreviewWidth || newH != m_PreviewHeight) {
				m_PreviewWidth = newW;
				m_PreviewHeight = newH;
				m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
			}

			uint32_t textureID = m_PreviewRenderer->GetPreviewTextureID();
			if (textureID > 0) {
				Image(textureID, viewportSize, true);
			} else {
				Size textSize = CalcTextSize("Preview Loading...");
				Position cursorPos(
					(viewportSize.x - textSize.x) * 0.5f,
					(viewportSize.y - textSize.y) * 0.5f
				);
				SetCursorPos(cursorPos);
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

		// Name bar with save button
		if (MaterialNameBar(m_EditingMaterial->GetName(), m_HasUnsavedChanges)) {
			SaveMaterial();
		}

		// All collapsible sections
		DrawBaseColorSection();
		DrawPBRPropertiesSection();
		DrawSurfaceSettingsSection();
		DrawTextureMapsSection();
		DrawLayeredTextureSection();
		DrawEmissionSection();
		DrawHeightMapSection();
		DrawDetailMapSection();
		DrawMaterialInfoSection();

		AddSpacing(SpacingValues::XXL);
	}

	// ============================================================================
	// BASE COLOR SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawBaseColorSection() {
		using namespace UI;

		Color accent = MatStyle::AccentPrimary();
		if (!CollapsibleSection("Base Color", m_SectionBaseColor, &accent))
			return;

		ScopedID sectionID("BaseColorSection");

		if (BeginSectionContent("##BaseColorContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			glm::vec4 albedo = m_EditingMaterial->GetAlbedo();
			Color albedoColor(albedo);
			if (PropertyColor4("Albedo Color", albedoColor, "Base color tint applied to the surface")) {
				m_EditingMaterial->SetAlbedo(glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a));
				MarkAsModified();
			}

			AddSpacing(SpacingValues::SM);

			MaterialTextureSlot("Albedo Map", m_EditingMaterial->GetAlbedoMap(), m_EditingMaterial->GetAlbedoPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAlbedoMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetAlbedoMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasAlbedoMap()) {
				const char* colorSpaces[] = { "sRGB", "Linear", "Linear Rec.709" };
				int cs = static_cast<int>(m_EditingMaterial->GetAlbedoColorSpace());
				if (PropertyDropdown("Color Space##Albedo", cs, colorSpaces, 3, "Texture color space interpretation")) {
					m_EditingMaterial->SetAlbedoColorSpace(static_cast<TextureColorSpace>(cs));
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// PBR PROPERTIES SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawPBRPropertiesSection() {
		using namespace UI;

		if (!CollapsibleSection("PBR Parameters", m_SectionPBR))
			return;

		ScopedID sectionID("PBRSection");

		if (BeginSectionContent("##PBRContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			float metallic = m_EditingMaterial->GetMetallic();
			if (PropertySlider("Metallic", metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
				m_EditingMaterial->SetMetallic(metallic);
				MarkAsModified();
			}

			float roughness = m_EditingMaterial->GetRoughness();
			if (PropertySlider("Roughness", roughness, 0.0f, 1.0f, "%.2f", "0 = Mirror, 1 = Diffuse")) {
				m_EditingMaterial->SetRoughness(roughness);
				MarkAsModified();
			}

			float specular = m_EditingMaterial->GetSpecular();
			if (PropertySlider("Specular", specular, 0.0f, 1.0f, "%.2f", "Fresnel reflectance at normal incidence")) {
				m_EditingMaterial->SetSpecular(specular);
				MarkAsModified();
			}

			AddSpacing(SpacingValues::XS);
			Separator();
			AddSpacing(SpacingValues::XS);

			float normalIntensity = m_EditingMaterial->GetNormalIntensity();
			if (PropertySlider("Normal Intensity", normalIntensity, 0.0f, 2.0f, "%.2f", "Strength of normal map effect")) {
				m_EditingMaterial->SetNormalIntensity(normalIntensity);
				MarkAsModified();
			}

			bool flipY = m_EditingMaterial->GetFlipNormalMapY();
			if (PropertyCheckbox("Flip Normal Y", flipY, "Invert green channel (DirectX-style normals)")) {
				m_EditingMaterial->SetFlipNormalMapY(flipY);
				MarkAsModified();
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// SURFACE SETTINGS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawSurfaceSettingsSection() {
		using namespace UI;

		if (!CollapsibleSection("Surface Settings", m_SectionSurface))
			return;

		ScopedID sectionID("SurfaceSection");

		if (BeginSectionContent("##SurfaceContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			const char* alphaModes[] = { "Opaque", "Cutoff", "Transparent" };
			int currentMode = static_cast<int>(m_EditingMaterial->GetAlphaMode());
			if (PropertyDropdown("Alpha Mode", currentMode, alphaModes, 3, "How transparency is handled")) {
				m_EditingMaterial->SetAlphaMode(static_cast<AlphaMode>(currentMode));
				MarkAsModified();
			}

			if (m_EditingMaterial->GetAlphaMode() == AlphaMode::Cutoff) {
				float cutoff = m_EditingMaterial->GetAlphaCutoff();
				if (PropertySlider("Alpha Cutoff", cutoff, 0.0f, 1.0f, "%.2f", "Pixels below this alpha are discarded")) {
					m_EditingMaterial->SetAlphaCutoff(cutoff);
					MarkAsModified();
				}
			}

			bool twoSided = m_EditingMaterial->IsTwoSided();
			if (PropertyCheckbox("Two Sided", twoSided, "Render both front and back faces")) {
				m_EditingMaterial->SetTwoSided(twoSided);
				MarkAsModified();
			}

			AddSpacing(SpacingValues::XS);
			Separator();
			AddSpacing(SpacingValues::XS);

			glm::vec2 tiling = m_EditingMaterial->GetUVTiling();
			if (PropertyVec2("UV Tiling", tiling, 0.01f, "Texture repeat count")) {
				m_EditingMaterial->SetUVTiling(tiling);
				MarkAsModified();
			}

			glm::vec2 offset = m_EditingMaterial->GetUVOffset();
			if (PropertyVec2("UV Offset", offset, 0.01f, "Texture position offset")) {
				m_EditingMaterial->SetUVOffset(offset);
				MarkAsModified();
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// TEXTURE MAPS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawTextureMapsSection() {
		using namespace UI;

		if (!CollapsibleSection("Texture Maps", m_SectionTextures))
			return;

		ScopedID sectionID("TextureMapsSection");

		if (BeginSectionContent("##TextureMapsContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			bool layeredActive = m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap();

			// Normal Map
			MaterialTextureSlot("Normal Map", m_EditingMaterial->GetNormalMap(), m_EditingMaterial->GetNormalPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetNormalMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetNormalMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasNormalMap()) {
				Indent(16.0f);
				const char* colorSpaces[] = { "sRGB", "Linear", "Linear Rec.709" };
				int cs = static_cast<int>(m_EditingMaterial->GetNormalColorSpace());
				if (PropertyDropdown("Color Space##Normal", cs, colorSpaces, 3, "Typically Linear for normal maps")) {
					m_EditingMaterial->SetNormalColorSpace(static_cast<TextureColorSpace>(cs));
					MarkAsModified();
				}
				Unindent(16.0f);
			}

			AddSpacing(SpacingValues::XS);

			// Metallic Map
			{
				ScopedDisabled disabled(layeredActive);
				MaterialTextureSlot("Metallic Map", m_EditingMaterial->GetMetallicMap(), m_EditingMaterial->GetMetallicPath(),
					[this](Ref<Texture2D> tex) { m_EditingMaterial->SetMetallicMap(tex); MarkAsModified(); },
					[this]() { m_EditingMaterial->SetMetallicMap(nullptr); MarkAsModified(); });

				if (m_EditingMaterial->HasMetallicMap() && !layeredActive) {
					Indent(16.0f);
					float mult = m_EditingMaterial->GetMetallicMultiplier();
					if (PropertySlider("Multiplier##MetallicTex", mult, 0.0f, 2.0f, "%.2f")) {
						m_EditingMaterial->SetMetallicMultiplier(mult);
						MarkAsModified();
					}
					Unindent(16.0f);
				}
			}

			AddSpacing(SpacingValues::XS);

			// Roughness Map
			{
				ScopedDisabled disabled(layeredActive);
				MaterialTextureSlot("Roughness Map", m_EditingMaterial->GetRoughnessMap(), m_EditingMaterial->GetRoughnessPath(),
					[this](Ref<Texture2D> tex) { m_EditingMaterial->SetRoughnessMap(tex); MarkAsModified(); },
					[this]() { m_EditingMaterial->SetRoughnessMap(nullptr); MarkAsModified(); });

				if (m_EditingMaterial->HasRoughnessMap() && !layeredActive) {
					Indent(16.0f);
					float mult = m_EditingMaterial->GetRoughnessMultiplier();
					if (PropertySlider("Multiplier##RoughnessTex", mult, 0.0f, 2.0f, "%.2f")) {
						m_EditingMaterial->SetRoughnessMultiplier(mult);
						MarkAsModified();
					}
					Unindent(16.0f);
				}
			}

			AddSpacing(SpacingValues::XS);

			// Specular Map
			MaterialTextureSlot("Specular Map", m_EditingMaterial->GetSpecularMap(), m_EditingMaterial->GetSpecularPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetSpecularMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetSpecularMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasSpecularMap()) {
				Indent(16.0f);
				float mult = m_EditingMaterial->GetSpecularMultiplier();
				if (PropertySlider("Multiplier##SpecularTex", mult, 0.0f, 2.0f, "%.2f")) {
					m_EditingMaterial->SetSpecularMultiplier(mult);
					MarkAsModified();
				}
				Unindent(16.0f);
			}

			AddSpacing(SpacingValues::XS);

			// AO Map
			{
				ScopedDisabled disabled(layeredActive);
				MaterialTextureSlot("Ambient Occlusion", m_EditingMaterial->GetAOMap(), m_EditingMaterial->GetAOPath(),
					[this](Ref<Texture2D> tex) { m_EditingMaterial->SetAOMap(tex); MarkAsModified(); },
					[this]() { m_EditingMaterial->SetAOMap(nullptr); MarkAsModified(); });

				if (m_EditingMaterial->HasAOMap() && !layeredActive) {
					Indent(16.0f);
					float mult = m_EditingMaterial->GetAOMultiplier();
					if (PropertySlider("Multiplier##AOTex", mult, 0.0f, 2.0f, "%.2f")) {
						m_EditingMaterial->SetAOMultiplier(mult);
						MarkAsModified();
					}
					Unindent(16.0f);
				}
			}

			if (layeredActive) {
				AddSpacing(SpacingValues::SM);
				TextWrapped("Metallic, Roughness and AO maps are overridden by the active ORM texture.", TextVariant::Muted);
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// LAYERED (ORM) TEXTURE SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawLayeredTextureSection() {
		using namespace UI;

		Color accent = MatStyle::AccentLayered();
		if (!CollapsibleSection("Layered Texture (ORM)", m_SectionLayered, &accent))
			return;

		ScopedID sectionID("LayeredTextureSection");

		if (BeginSectionContent("##LayeredContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			TextWrapped("Pack Metallic, Roughness and AO into a single texture to reduce VRAM usage.", TextVariant::Muted);
			AddSpacing(SpacingValues::SM);

			bool useLayered = m_EditingMaterial->GetUseLayeredMap();
			if (PropertyCheckbox("Enable ORM", useLayered, "Use packed ORM texture instead of separate maps")) {
				m_EditingMaterial->SetUseLayeredMap(useLayered);
				MarkAsModified();
			}

			if (m_EditingMaterial->GetUseLayeredMap()) {
				AddSpacing(SpacingValues::SM);

				MaterialTextureSlot("ORM Texture", m_EditingMaterial->GetLayeredMap(), m_EditingMaterial->GetLayeredPath(),
					[this](Ref<Texture2D> tex) { m_EditingMaterial->SetLayeredMap(tex); MarkAsModified(); },
					[this]() { m_EditingMaterial->SetLayeredMap(nullptr); MarkAsModified(); });

				if (m_EditingMaterial->HasLayeredMap()) {
					AddSpacing(SpacingValues::SM);

					const char* colorSpaces[] = { "sRGB", "Linear", "Linear Rec.709" };
				 int cs = static_cast<int>(m_EditingMaterial->GetLayeredColorSpace());
					if (PropertyDropdown("Color Space##Layered", cs, colorSpaces, 3, "Typically Linear for data textures")) {
						m_EditingMaterial->SetLayeredColorSpace(static_cast<TextureColorSpace>(cs));
						MarkAsModified();
					}

					AddSpacing(SpacingValues::XS);

					const char* channelNames[] = { "Red (R)", "Green (G)", "Blue (B)" };

					{
						ScopedColor labelColor(ColorVar::Text, MatStyle::ChannelR());
						int chMetallic = m_EditingMaterial->GetLayeredChannelMetallic();
						if (PropertyDropdown("Metallic Ch.", chMetallic, channelNames, 3, "Channel containing metallic data")) {
							m_EditingMaterial->SetLayeredChannelMetallic(chMetallic);
							MarkAsModified();
						}
					}
					{
						ScopedColor labelColor(ColorVar::Text, MatStyle::ChannelG());
						int chRoughness = m_EditingMaterial->GetLayeredChannelRoughness();
						if (PropertyDropdown("Roughness Ch.", chRoughness, channelNames, 3, "Channel containing roughness data")) {
							m_EditingMaterial->SetLayeredChannelRoughness(chRoughness);
							MarkAsModified();
						}
					}
					{
						ScopedColor labelColor(ColorVar::Text, MatStyle::ChannelB());
						int chAO = m_EditingMaterial->GetLayeredChannelAO();
						if (PropertyDropdown("AO Ch.", chAO, channelNames, 3, "Channel containing ambient occlusion data")) {
							m_EditingMaterial->SetLayeredChannelAO(chAO);
							MarkAsModified();
						}
					}

					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);

					float metallicMult = m_EditingMaterial->GetMetallicMultiplier();
					if (PropertySlider("Metallic Mult", metallicMult, 0.0f, 2.0f, "%.2f", "Scale metallic from ORM")) {
						m_EditingMaterial->SetMetallicMultiplier(metallicMult);
						MarkAsModified();
					}
					float roughnessMult = m_EditingMaterial->GetRoughnessMultiplier();
					if (PropertySlider("Roughness Mult", roughnessMult, 0.0f, 2.0f, "%.2f", "Scale roughness from ORM")) {
						m_EditingMaterial->SetRoughnessMultiplier(roughnessMult);
						MarkAsModified();
					}
					float aoMult = m_EditingMaterial->GetAOMultiplier();
					if (PropertySlider("AO Mult", aoMult, 0.0f, 2.0f, "%.2f", "Scale AO from ORM")) {
						m_EditingMaterial->SetAOMultiplier(aoMult);
						MarkAsModified();
					}

					if (m_EditingMaterial->HasMetallicMap() || m_EditingMaterial->HasRoughnessMap() || m_EditingMaterial->HasAOMap()) {
						AddSpacing(SpacingValues::SM);
						TextWrapped("ORM texture overrides separate Metallic, Roughness and AO maps.", TextVariant::Warning);
					}
				}
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// EMISSION SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawEmissionSection() {
		using namespace UI;

		Color accent = MatStyle::AccentEmission();
		if (!CollapsibleSection("Emission", m_SectionEmission, &accent))
			return;

		ScopedID sectionID("EmissionSection");

		if (BeginSectionContent("##EmissionContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			glm::vec3 emissionColor = m_EditingMaterial->GetEmissionColor();
			Color3 color(emissionColor);
			if (PropertyColor("Emission Color", color, "Color of emitted light")) {
				m_EditingMaterial->SetEmissionColor(color);
				MarkAsModified();
			}

			float intensity = m_EditingMaterial->GetEmissionIntensity();
			if (PropertySlider("Intensity", intensity, 0.0f, 100.0f, "%.1f", "Emission brightness")) {
				m_EditingMaterial->SetEmissionIntensity(intensity);
				MarkAsModified();
			}

			AddSpacing(SpacingValues::XS);

			MaterialTextureSlot("Emission Map", m_EditingMaterial->GetEmissionMap(), m_EditingMaterial->GetEmissionPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetEmissionMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetEmissionMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasEmissionMap()) {
				Indent(16.0f);
				const char* colorSpaces[] = { "sRGB", "Linear", "Linear Rec.709" };
				int cs = static_cast<int>(m_EditingMaterial->GetEmissionColorSpace());
				if (PropertyDropdown("Color Space##Emission", cs, colorSpaces, 3, "Emission texture color space")) {
					m_EditingMaterial->SetEmissionColorSpace(static_cast<TextureColorSpace>(cs));
					MarkAsModified();
				}
				Unindent(16.0f);
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// HEIGHT MAP / PARALLAX SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawHeightMapSection() {
		using namespace UI;

		Color accent = MatStyle::AccentHeight();
		if (!CollapsibleSection("Height / Parallax", m_SectionHeight, &accent))
			return;

		ScopedID sectionID("HeightMapSection");

		if (BeginSectionContent("##HeightContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			MaterialTextureSlot("Height Map", m_EditingMaterial->GetHeightMap(), m_EditingMaterial->GetHeightPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetHeightMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetHeightMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasHeightMap()) {
				Indent(16.0f);
				float heightScale = m_EditingMaterial->GetHeightScale();
				if (PropertySlider("Height Scale", heightScale, 0.0f, 0.5f, "%.3f", "Parallax displacement depth")) {
					m_EditingMaterial->SetHeightScale(heightScale);
					MarkAsModified();
				}
				Unindent(16.0f);
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// DETAIL NORMAL MAP SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawDetailMapSection() {
		using namespace UI;

		if (!CollapsibleSection("Detail Normal", m_SectionDetail))
			return;

		ScopedID sectionID("DetailMapSection");

		if (BeginSectionContent("##DetailContent", MatStyle::BgSection())) {
			AddSpacing(SpacingValues::SM);

			TextWrapped("High-frequency surface detail blended over the main normal map with independent UV tiling.", TextVariant::Muted);
			AddSpacing(SpacingValues::SM);

			MaterialTextureSlot("Detail Normal", m_EditingMaterial->GetDetailNormalMap(), m_EditingMaterial->GetDetailNormalPath(),
				[this](Ref<Texture2D> tex) { m_EditingMaterial->SetDetailNormalMap(tex); MarkAsModified(); },
				[this]() { m_EditingMaterial->SetDetailNormalMap(nullptr); MarkAsModified(); });

			if (m_EditingMaterial->HasDetailNormalMap()) {
				Indent(16.0f);

				float detailScale = m_EditingMaterial->GetDetailNormalScale();
				if (PropertySlider("Intensity", detailScale, 0.0f, 2.0f, "%.2f", "Strength of detail normal")) {
					m_EditingMaterial->SetDetailNormalScale(detailScale);
					MarkAsModified();
				}

				glm::vec2 detailTiling = m_EditingMaterial->GetDetailUVTiling();
				if (PropertyVec2("UV Tiling", detailTiling, 0.1f, "Independent tiling for detail texture")) {
					m_EditingMaterial->SetDetailUVTiling(detailTiling);
					MarkAsModified();
				}

				Unindent(16.0f);
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
	}

	// ============================================================================
	// MATERIAL INFO SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawMaterialInfoSection() {
		using namespace UI;

		if (!CollapsibleSection("Material Info", m_SectionInfo))
			return;

		ScopedID sectionID("MaterialInfoSection");

		if (BeginSectionContent("##InfoContent", MatStyle::InfoBg())) {
			AddSpacing(SpacingValues::SM);

			InfoRow("Asset ID:", "%llu", (unsigned long long)(uint64_t)m_EditingMaterial->GetID());

			if (!m_EditingMaterial->GetPath().empty()) {
				TextStyled("File:", TextVariant::Muted);
				SameLine(130.0f);
				TextWrapped(m_EditingMaterial->GetPath().string());
			}

			InfoRow("Textures:", "%d loaded", m_EditingMaterial->GetTextureCount());
			InfoRow("Alpha:", "%s", AlphaModeToString(m_EditingMaterial->GetAlphaMode()));

			AddSpacing(SpacingValues::XS);

			// Active features summary
			if (m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap()) {
				TextColored(MatStyle::AccentLayered(), "ORM Active");
				SameLine();
				TextStyled("(saves 2 texture slots)", TextVariant::Muted);
			}

			if (m_EditingMaterial->IsTwoSided()) {
				TextColored(MatStyle::StatusUnsaved(), "Two-Sided");
			}

			if (m_EditingMaterial->HasDetailNormalMap()) {
				TextColored(MatStyle::AccentPrimary(), "Detail Normal (%.0fx%.0f tiling)",
					m_EditingMaterial->GetDetailUVTiling().x,
					m_EditingMaterial->GetDetailUVTiling().y);
			}

			if (m_EditingMaterial->HasHeightMap()) {
				TextColored(MatStyle::AccentHeight(), "Parallax (scale: %.3f)", m_EditingMaterial->GetHeightScale());
			}

			if (m_EditingMaterial->GetFlipNormalMapY()) {
				TextColored(MatStyle::AccentLayered(), "Normal Y-Flipped (DirectX)");
			}

			AddSpacing(SpacingValues::SM);
		}
		EndSectionContent();
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
		if (m_HasUnsavedChanges) {
			SaveMaterial();
		}
		return true;
	}

} // namespace Lunex