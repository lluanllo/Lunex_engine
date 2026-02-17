/**
 * @file MaterialEditorPanel.cpp
 * @brief Material Editor Panel - AAA Quality Implementation using Lunex UI Framework
 *
 * Design principles:
 * - Collapsible sections with clear visual hierarchy (inspired by Unreal Engine)
 * - No duplicate controls - each property appears exactly once
 * - Logical grouping: Base Color > PBR Parameters > Textures > Advanced
 * - Compact texture slots that don't waste vertical space
 * - Contextual controls: sub-properties only appear when relevant
 * - Accessible: proper contrast, tooltips, keyboard-friendly
 * - Clean separation between preview (left) and properties (right)
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
		constexpr float PreviewMinWidth = 400.0f;
		constexpr float TextureThumbnailSize = 48.0f;
		constexpr float TextureSlotHeight = 56.0f;
		constexpr float SectionHeaderHeight = 28.0f;

		UI::Color BgPanel()              { return UI::Color(0.12f, 0.12f, 0.14f, 1.0f); }
		UI::Color BgPreview()            { return UI::Color(0.07f, 0.07f, 0.08f, 1.0f); }
		UI::Color BgSection()            { return UI::Color(0.15f, 0.15f, 0.17f, 1.0f); }
		UI::Color BgSectionHeader()      { return UI::Color(0.17f, 0.18f, 0.20f, 1.0f); }
		UI::Color BgSectionHeaderHover() { return UI::Color(0.20f, 0.21f, 0.24f, 1.0f); }
		UI::Color BgTextureSlot()        { return UI::Color(0.11f, 0.11f, 0.13f, 1.0f); }
		UI::Color BgTextureSlotHover()   { return UI::Color(0.14f, 0.14f, 0.16f, 1.0f); }
		UI::Color BorderSubtle()         { return UI::Color(0.22f, 0.22f, 0.25f, 1.0f); }
		UI::Color BorderSection()        { return UI::Color(0.18f, 0.18f, 0.21f, 1.0f); }
		UI::Color AccentPrimary()        { return UI::Color(0.26f, 0.59f, 0.98f, 1.0f); }
		UI::Color AccentLayered()        { return UI::Color(0.90f, 0.55f, 0.15f, 1.0f); }
		UI::Color AccentEmission()       { return UI::Color(0.95f, 0.80f, 0.20f, 1.0f); }
		UI::Color AccentHeight()         { return UI::Color(0.55f, 0.75f, 0.35f, 1.0f); }
		UI::Color StatusSaved()          { return UI::Color(0.30f, 0.80f, 0.30f, 1.0f); }
		UI::Color StatusUnsaved()        { return UI::Color(0.95f, 0.75f, 0.20f, 1.0f); }
		UI::Color ButtonDanger()         { return UI::Color(0.55f, 0.20f, 0.20f, 1.0f); }
		UI::Color ButtonDangerHover()    { return UI::Color(0.70f, 0.25f, 0.25f, 1.0f); }
		UI::Color TextMuted()            { return UI::Color(0.45f, 0.45f, 0.50f, 1.0f); }
		UI::Color TextHeader()           { return UI::Color(0.90f, 0.90f, 0.92f, 1.0f); }
		UI::Color TextLabel()            { return UI::Color(0.70f, 0.70f, 0.75f, 1.0f); }
		UI::Color ChannelR()             { return UI::Color(0.85f, 0.30f, 0.30f, 1.0f); }
		UI::Color ChannelG()             { return UI::Color(0.30f, 0.80f, 0.30f, 1.0f); }
		UI::Color ChannelB()             { return UI::Color(0.30f, 0.45f, 0.90f, 1.0f); }
		UI::Color InfoBg()               { return UI::Color(0.10f, 0.11f, 0.13f, 1.0f); }
		UI::Color SectionArrow()         { return UI::Color(0.50f, 0.50f, 0.55f, 1.0f); }
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

		// Auto-expand sections that have content
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
		}
		else {
			ImGui::PopStyleVar(2);
		}
		ImGui::End();

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
		float previewWidth = std::max(availSize.x * 0.45f, MaterialEditorStyle::PreviewMinWidth);

		ScopedStyle layoutStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// Left column: Preview viewport
		{
			ScopedColor previewBg(ImGuiCol_ChildBg, MaterialEditorStyle::BgPreview());

			if (BeginChild("##PreviewArea", Size(previewWidth, availSize.y), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
				DrawPreviewViewport();
			}
			EndChild();
		}

		SameLine();

		// Right column: Properties panel with vertical scroll
		{
			ScopedColor propsBg(ImGuiCol_ChildBg, MaterialEditorStyle::BgPanel());
			ScopedStyle propsPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

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
				if (MenuItem("Save", "Ctrl+S")) {
					SaveMaterial();
				}
				if (MenuItem("Save As...", nullptr, false, false)) {
					// TODO
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

				Separator();

				if (BeginMenu("Sections")) {
					ImGui::Checkbox("Base Color & Albedo", &m_SectionBaseColor);
					ImGui::Checkbox("PBR Parameters", &m_SectionPBR);
					ImGui::Checkbox("Surface Settings", &m_SectionSurface);
					ImGui::Checkbox("Texture Maps", &m_SectionTextures);
					ImGui::Checkbox("Layered (ORM)", &m_SectionLayered);
					ImGui::Checkbox("Emission", &m_SectionEmission);
					ImGui::Checkbox("Height / Parallax", &m_SectionHeight);
					ImGui::Checkbox("Detail Normal", &m_SectionDetail);
					ImGui::Checkbox("Material Info", &m_SectionInfo);
					EndMenu();
				}

				EndMenu();
			}

			// Status indicator right-aligned
			float statusWidth = 100.0f;
			float availWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - statusWidth);

			if (m_HasUnsavedChanges) {
				ScopedColor statusColor(ImGuiCol_Text, MaterialEditorStyle::StatusUnsaved());
				UI::Text("Modified");
			}
			else {
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
			}
			else {
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
	// COLLAPSIBLE SECTION HEADER
	// ============================================================================

	bool MaterialEditorPanel::DrawSectionHeader(const char* label, bool& isOpen, const ImVec4* accentColor) {
		using namespace UI;

		ImGui::PushID(label);

		float fullWidth = ImGui::GetContentRegionAvail().x;
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		float headerH = MaterialEditorStyle::SectionHeaderHeight;

		// Background
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 headerMin = cursorPos;
		ImVec2 headerMax = ImVec2(cursorPos.x + fullWidth, cursorPos.y + headerH);

		bool hovered = ImGui::IsMouseHoveringRect(headerMin, headerMax);
		UI::Color bgColor = hovered ? MaterialEditorStyle::BgSectionHeaderHover() : MaterialEditorStyle::BgSectionHeader();
		drawList->AddRectFilled(headerMin, headerMax, bgColor.ToImU32(), 0.0f);

		// Bottom border
		drawList->AddLine(
			ImVec2(headerMin.x, headerMax.y),
			ImVec2(headerMax.x, headerMax.y),
			MaterialEditorStyle::BorderSection().ToImU32());

		// Accent bar on the left
		if (accentColor) {
			drawList->AddRectFilled(
				headerMin,
				ImVec2(headerMin.x + 3.0f, headerMax.y),
				ImGui::ColorConvertFloat4ToU32(*accentColor));
		}

		// Arrow indicator
		float arrowX = headerMin.x + (accentColor ? 12.0f : 8.0f);
		float arrowY = headerMin.y + (headerH - ImGui::GetTextLineHeight()) * 0.5f;
		ImGui::SetCursorScreenPos(ImVec2(arrowX, arrowY));
		{
			ScopedColor arrowColor(ImGuiCol_Text, MaterialEditorStyle::SectionArrow());
			UI::Text(isOpen ? "v" : ">");
		}

		// Label text
		float textX = arrowX + 18.0f;
		ImGui::SetCursorScreenPos(ImVec2(textX, arrowY));
		{
			ScopedColor headerTextColor(ImGuiCol_Text, MaterialEditorStyle::TextHeader());
			UI::Text("%s", label);
		}

		// Invisible button to handle click
		ImGui::SetCursorScreenPos(headerMin);
		if (ImGui::InvisibleButton("##SectionToggle", ImVec2(fullWidth, headerH))) {
			isOpen = !isOpen;
		}

		ImGui::PopID();

		return isOpen;
	}

	// ============================================================================
	// PROPERTIES PANEL
	// ============================================================================

	void MaterialEditorPanel::DrawPropertiesPanel() {
		if (!m_EditingMaterial) return;

		using namespace UI;

		// Material name header bar
		{
			float fullWidth = ImGui::GetContentRegionAvail().x;
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float nameBarH = 32.0f;
			drawList->AddRectFilled(
				cursorPos,
				ImVec2(cursorPos.x + fullWidth, cursorPos.y + nameBarH),
				UI::Color(0.14f, 0.14f, 0.16f, 1.0f).ToImU32());
			drawList->AddLine(
				ImVec2(cursorPos.x, cursorPos.y + nameBarH),
				ImVec2(cursorPos.x + fullWidth, cursorPos.y + nameBarH),
				MaterialEditorStyle::BorderSubtle().ToImU32());

			// Accent bar
			drawList->AddRectFilled(
				cursorPos,
				ImVec2(cursorPos.x + 3.0f, cursorPos.y + nameBarH),
				MaterialEditorStyle::AccentPrimary().ToImU32());

			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 12.0f, cursorPos.y + (nameBarH - ImGui::GetTextLineHeight()) * 0.5f));
			{
				ScopedColor headerColor(ImGuiCol_Text, MaterialEditorStyle::TextHeader());
				UI::Text("%s", m_EditingMaterial->GetName().c_str());
			}

			// Save button right-aligned
			float saveBtnW = 50.0f;
			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + fullWidth - saveBtnW - 8.0f, cursorPos.y + 4.0f));
			if (m_HasUnsavedChanges) {
				ScopedColor btnColors({
					{ImGuiCol_Button, MaterialEditorStyle::AccentPrimary().Darker(0.1f)},
					{ImGuiCol_ButtonHovered, MaterialEditorStyle::AccentPrimary()},
					{ImGuiCol_ButtonActive, MaterialEditorStyle::AccentPrimary().Darker(0.2f)}
				});
				if (ImGui::SmallButton("Save")) {
					SaveMaterial();
				}
			}

			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + nameBarH + 1.0f));
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

		ImVec4 accent = MaterialEditorStyle::AccentPrimary().ToImVec4();
		if (!DrawSectionHeader("Base Color", m_SectionBaseColor, &accent))
			return;

		ScopedID sectionID("BaseColorSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##BaseColorContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			{
				glm::vec4 albedo = m_EditingMaterial->GetAlbedo();
				UI::Color albedoColor(albedo);
				if (PropertyColor4("Albedo Color", albedoColor, "Base color tint applied to the surface")) {
					m_EditingMaterial->SetAlbedo(glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a));
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::SM);

			DrawTextureSlotCompact("Albedo Map", m_EditingMaterial->GetAlbedoMap(), m_EditingMaterial->GetAlbedoPath(),
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
		ImGui::EndChild();
	}

	// ============================================================================
	// PBR PROPERTIES SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawPBRPropertiesSection() {
		using namespace UI;

		if (!DrawSectionHeader("PBR Parameters", m_SectionPBR))
			return;

		ScopedID sectionID("PBRSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##PBRContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			{
				float metallic = m_EditingMaterial->GetMetallic();
				if (PropertySlider("Metallic", metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
					m_EditingMaterial->SetMetallic(metallic);
					MarkAsModified();
				}
			}

			{
				float roughness = m_EditingMaterial->GetRoughness();
				if (PropertySlider("Roughness", roughness, 0.0f, 1.0f, "%.2f", "0 = Mirror, 1 = Diffuse")) {
					m_EditingMaterial->SetRoughness(roughness);
					MarkAsModified();
				}
			}

			{
				float specular = m_EditingMaterial->GetSpecular();
				if (PropertySlider("Specular", specular, 0.0f, 1.0f, "%.2f", "Fresnel reflectance at normal incidence")) {
					m_EditingMaterial->SetSpecular(specular);
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::XS);
			Separator();
			AddSpacing(SpacingValues::XS);

			{
				float normalIntensity = m_EditingMaterial->GetNormalIntensity();
				if (PropertySlider("Normal Intensity", normalIntensity, 0.0f, 2.0f, "%.2f", "Strength of normal map effect")) {
					m_EditingMaterial->SetNormalIntensity(normalIntensity);
					MarkAsModified();
				}
			}

			{
				bool flipY = m_EditingMaterial->GetFlipNormalMapY();
				if (PropertyCheckbox("Flip Normal Y", flipY, "Invert green channel (DirectX-style normals)")) {
					m_EditingMaterial->SetFlipNormalMapY(flipY);
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::SM);
		}
		ImGui::EndChild();
	}

	// ============================================================================
	// SURFACE SETTINGS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawSurfaceSettingsSection() {
		using namespace UI;

		if (!DrawSectionHeader("Surface Settings", m_SectionSurface))
			return;

		ScopedID sectionID("SurfaceSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##SurfaceContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			{
				const char* alphaModes[] = { "Opaque", "Cutoff", "Transparent" };
				int currentMode = static_cast<int>(m_EditingMaterial->GetAlphaMode());
				if (PropertyDropdown("Alpha Mode", currentMode, alphaModes, 3, "How transparency is handled")) {
					m_EditingMaterial->SetAlphaMode(static_cast<AlphaMode>(currentMode));
					MarkAsModified();
				}
			}

			if (m_EditingMaterial->GetAlphaMode() == AlphaMode::Cutoff) {
				float cutoff = m_EditingMaterial->GetAlphaCutoff();
				if (PropertySlider("Alpha Cutoff", cutoff, 0.0f, 1.0f, "%.2f", "Pixels below this alpha are discarded")) {
					m_EditingMaterial->SetAlphaCutoff(cutoff);
					MarkAsModified();
				}
			}

			{
				bool twoSided = m_EditingMaterial->IsTwoSided();
				if (PropertyCheckbox("Two Sided", twoSided, "Render both front and back faces")) {
					m_EditingMaterial->SetTwoSided(twoSided);
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::XS);
			Separator();
			AddSpacing(SpacingValues::XS);

			{
				glm::vec2 tiling = m_EditingMaterial->GetUVTiling();
				if (PropertyVec2("UV Tiling", tiling, 0.01f, "Texture repeat count")) {
					m_EditingMaterial->SetUVTiling(tiling);
					MarkAsModified();
				}
			}

			{
				glm::vec2 offset = m_EditingMaterial->GetUVOffset();
				if (PropertyVec2("UV Offset", offset, 0.01f, "Texture position offset")) {
					m_EditingMaterial->SetUVOffset(offset);
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::SM);
		}
		ImGui::EndChild();
	}

	// ============================================================================
	// TEXTURE MAPS SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawTextureMapsSection() {
		using namespace UI;

		if (!DrawSectionHeader("Texture Maps", m_SectionTextures))
			return;

		ScopedID sectionID("TextureMapsSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##TextureMapsContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			bool layeredActive = m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap();

			// Normal Map
			DrawTextureSlotCompact("Normal Map", m_EditingMaterial->GetNormalMap(), m_EditingMaterial->GetNormalPath(),
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

				DrawTextureSlotCompact("Metallic Map", m_EditingMaterial->GetMetallicMap(), m_EditingMaterial->GetMetallicPath(),
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

				DrawTextureSlotCompact("Roughness Map", m_EditingMaterial->GetRoughnessMap(), m_EditingMaterial->GetRoughnessPath(),
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
			DrawTextureSlotCompact("Specular Map", m_EditingMaterial->GetSpecularMap(), m_EditingMaterial->GetSpecularPath(),
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

				DrawTextureSlotCompact("Ambient Occlusion", m_EditingMaterial->GetAOMap(), m_EditingMaterial->GetAOPath(),
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
		ImGui::EndChild();
	}

	// ============================================================================
	// LAYERED (ORM) TEXTURE SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawLayeredTextureSection() {
		using namespace UI;

		ImVec4 accent = MaterialEditorStyle::AccentLayered().ToImVec4();
		if (!DrawSectionHeader("Layered Texture (ORM)", m_SectionLayered, &accent))
			return;

		ScopedID sectionID("LayeredTextureSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##LayeredContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			TextWrapped("Pack Metallic, Roughness and AO into a single texture to reduce VRAM usage.", TextVariant::Muted);
			AddSpacing(SpacingValues::SM);

			{
				bool useLayered = m_EditingMaterial->GetUseLayeredMap();
				if (PropertyCheckbox("Enable ORM", useLayered, "Use packed ORM texture instead of separate maps")) {
					m_EditingMaterial->SetUseLayeredMap(useLayered);
					MarkAsModified();
				}
			}

			if (m_EditingMaterial->GetUseLayeredMap()) {
				AddSpacing(SpacingValues::SM);

				DrawTextureSlotCompact("ORM Texture", m_EditingMaterial->GetLayeredMap(), m_EditingMaterial->GetLayeredPath(),
					[this](Ref<Texture2D> tex) { m_EditingMaterial->SetLayeredMap(tex); MarkAsModified(); },
					[this]() { m_EditingMaterial->SetLayeredMap(nullptr); MarkAsModified(); });

				if (m_EditingMaterial->HasLayeredMap()) {
					AddSpacing(SpacingValues::SM);

					{
						const char* colorSpaces[] = { "sRGB", "Linear", "Linear Rec.709" };
					 int cs = static_cast<int>(m_EditingMaterial->GetLayeredColorSpace());
						if (PropertyDropdown("Color Space##Layered", cs, colorSpaces, 3, "Typically Linear for data textures")) {
							m_EditingMaterial->SetLayeredColorSpace(static_cast<TextureColorSpace>(cs));
							MarkAsModified();
						}
					}

					AddSpacing(SpacingValues::XS);

					const char* channelNames[] = { "Red (R)", "Green (G)", "Blue (B)" };

					{
						int chMetallic = m_EditingMaterial->GetLayeredChannelMetallic();
						ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelR());
						if (PropertyDropdown("Metallic Ch.", chMetallic, channelNames, 3, "Channel containing metallic data")) {
							m_EditingMaterial->SetLayeredChannelMetallic(chMetallic);
							MarkAsModified();
						}
					}
					{
						int chRoughness = m_EditingMaterial->GetLayeredChannelRoughness();
						ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelG());
						if (PropertyDropdown("Roughness Ch.", chRoughness, channelNames, 3, "Channel containing roughness data")) {
							m_EditingMaterial->SetLayeredChannelRoughness(chRoughness);
							MarkAsModified();
						}
					}
					{
						int chAO = m_EditingMaterial->GetLayeredChannelAO();
						ScopedColor labelColor(ImGuiCol_Text, MaterialEditorStyle::ChannelB());
						if (PropertyDropdown("AO Ch.", chAO, channelNames, 3, "Channel containing ambient occlusion data")) {
							m_EditingMaterial->SetLayeredChannelAO(chAO);
							MarkAsModified();
						}
					}

					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);

					{
						float metallicMult = m_EditingMaterial->GetMetallicMultiplier();
						if (PropertySlider("Metallic Mult", metallicMult, 0.0f, 2.0f, "%.2f", "Scale metallic from ORM")) {
							m_EditingMaterial->SetMetallicMultiplier(metallicMult);
							MarkAsModified();
						}
					}
					{
						float roughnessMult = m_EditingMaterial->GetRoughnessMultiplier();
						if (PropertySlider("Roughness Mult", roughnessMult, 0.0f, 2.0f, "%.2f", "Scale roughness from ORM")) {
							m_EditingMaterial->SetRoughnessMultiplier(roughnessMult);
							MarkAsModified();
						}
					}
					{
						float aoMult = m_EditingMaterial->GetAOMultiplier();
						if (PropertySlider("AO Mult", aoMult, 0.0f, 2.0f, "%.2f", "Scale AO from ORM")) {
							m_EditingMaterial->SetAOMultiplier(aoMult);
							MarkAsModified();
						}
					}

					if (m_EditingMaterial->HasMetallicMap() || m_EditingMaterial->HasRoughnessMap() || m_EditingMaterial->HasAOMap()) {
						AddSpacing(SpacingValues::SM);
						TextWrapped("ORM texture overrides separate Metallic, Roughness and AO maps.", TextVariant::Warning);
					}
				}
			}

			AddSpacing(SpacingValues::SM);
		}
		ImGui::EndChild();
	}

	// ============================================================================
	// EMISSION SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawEmissionSection() {
		using namespace UI;

		ImVec4 accent = MaterialEditorStyle::AccentEmission().ToImVec4();
		if (!DrawSectionHeader("Emission", m_SectionEmission, &accent))
			return;

		ScopedID sectionID("EmissionSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##EmissionContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			{
				glm::vec3 emissionColor = m_EditingMaterial->GetEmissionColor();
				UI::Color3 color(emissionColor);
				if (PropertyColor("Emission Color", color, "Color of emitted light")) {
					m_EditingMaterial->SetEmissionColor(color);
					MarkAsModified();
				}
			}

			{
				float intensity = m_EditingMaterial->GetEmissionIntensity();
				if (PropertySlider("Intensity", intensity, 0.0f, 100.0f, "%.1f", "Emission brightness")) {
					m_EditingMaterial->SetEmissionIntensity(intensity);
					MarkAsModified();
				}
			}

			AddSpacing(SpacingValues::XS);

			DrawTextureSlotCompact("Emission Map", m_EditingMaterial->GetEmissionMap(), m_EditingMaterial->GetEmissionPath(),
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
		ImGui::EndChild();
	}

	// ============================================================================
	// HEIGHT MAP / PARALLAX SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawHeightMapSection() {
		using namespace UI;

		ImVec4 accent = MaterialEditorStyle::AccentHeight().ToImVec4();
		if (!DrawSectionHeader("Height / Parallax", m_SectionHeight, &accent))
			return;

		ScopedID sectionID("HeightMapSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##HeightContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			DrawTextureSlotCompact("Height Map", m_EditingMaterial->GetHeightMap(), m_EditingMaterial->GetHeightPath(),
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
		ImGui::EndChild();
	}

	// ============================================================================
	// DETAIL NORMAL MAP SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawDetailMapSection() {
		using namespace UI;

		if (!DrawSectionHeader("Detail Normal", m_SectionDetail))
			return;

		ScopedID sectionID("DetailMapSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::BgSection());

		if (ImGui::BeginChild("##DetailContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			TextWrapped("High-frequency surface detail blended over the main normal map with independent UV tiling.", TextVariant::Muted);
			AddSpacing(SpacingValues::SM);

			DrawTextureSlotCompact("Detail Normal", m_EditingMaterial->GetDetailNormalMap(), m_EditingMaterial->GetDetailNormalPath(),
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
		ImGui::EndChild();
	}

	// ============================================================================
	// MATERIAL INFO SECTION
	// ============================================================================

	void MaterialEditorPanel::DrawMaterialInfoSection() {
		using namespace UI;

		if (!DrawSectionHeader("Material Info", m_SectionInfo))
			return;

		ScopedID sectionID("MaterialInfoSection");
		ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
		ScopedColor bg(ImGuiCol_ChildBg, MaterialEditorStyle::InfoBg());

		if (ImGui::BeginChild("##InfoContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar)) {
			AddSpacing(SpacingValues::SM);

			auto DrawInfoRow = [](const char* label, const char* fmt, ...) {
				ImGui::PushStyleColor(ImGuiCol_Text, MaterialEditorStyle::TextLabel().ToImVec4());
				ImGui::Text("%s", label);
				ImGui::PopStyleColor();
				ImGui::SameLine(130.0f);

				va_list args;
				va_start(args, fmt);
				ImGui::TextV(fmt, args);
				va_end(args);
			};

			DrawInfoRow("Asset ID:", "%llu", (unsigned long long)(uint64_t)m_EditingMaterial->GetID());

			if (!m_EditingMaterial->GetPath().empty()) {
				TextStyled("File:", TextVariant::Muted);
				SameLine(130.0f);
				TextWrapped(m_EditingMaterial->GetPath().string());
			}

			DrawInfoRow("Textures:", "%d loaded", m_EditingMaterial->GetTextureCount());
			DrawInfoRow("Alpha:", "%s", AlphaModeToString(m_EditingMaterial->GetAlphaMode()));

			AddSpacing(SpacingValues::XS);

			// Active features summary
			if (m_EditingMaterial->GetUseLayeredMap() && m_EditingMaterial->HasLayeredMap()) {
				TextColored(MaterialEditorStyle::AccentLayered(), "ORM Active");
				SameLine();
				TextStyled("(saves 2 texture slots)", TextVariant::Muted);
			}

			if (m_EditingMaterial->IsTwoSided()) {
				TextColored(MaterialEditorStyle::StatusUnsaved(), "Two-Sided");
			}

			if (m_EditingMaterial->HasDetailNormalMap()) {
				TextColored(MaterialEditorStyle::AccentPrimary(), "Detail Normal (%.0fx%.0f tiling)",
					m_EditingMaterial->GetDetailUVTiling().x,
					m_EditingMaterial->GetDetailUVTiling().y);
			}

			if (m_EditingMaterial->HasHeightMap()) {
				TextColored(MaterialEditorStyle::AccentHeight(), "Parallax (scale: %.3f)", m_EditingMaterial->GetHeightScale());
			}

			if (m_EditingMaterial->GetFlipNormalMapY()) {
				TextColored(MaterialEditorStyle::AccentLayered(), "Normal Y-Flipped (DirectX)");
			}

			AddSpacing(SpacingValues::SM);
		}
		ImGui::EndChild();
	}

	// ============================================================================
	// COMPACT TEXTURE SLOT
	// ============================================================================

	void MaterialEditorPanel::DrawTextureSlotCompact(
		const std::string& label,
		Ref<Texture2D> texture,
		const std::string& path,
		std::function<void(Ref<Texture2D>)> onTextureSet,
		std::function<void()> onTextureClear) {

		using namespace UI;

		ScopedID slotID(label);

		float slotWidth = ImGui::GetContentRegionAvail().x;
		float slotH = MaterialEditorStyle::TextureSlotHeight;
		float thumbSize = MaterialEditorStyle::TextureThumbnailSize;

		ImVec2 startPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 slotMin = startPos;
		ImVec2 slotMax = ImVec2(startPos.x + slotWidth, startPos.y + slotH);

		bool slotHovered = ImGui::IsMouseHoveringRect(slotMin, slotMax);
		UI::Color slotBg = slotHovered ? MaterialEditorStyle::BgTextureSlotHover() : MaterialEditorStyle::BgTextureSlot();
		drawList->AddRectFilled(slotMin, slotMax, slotBg.ToImU32(), 4.0f);
		drawList->AddRect(slotMin, slotMax, MaterialEditorStyle::BorderSubtle().WithAlpha(0.3f).ToImU32(), 4.0f);

		float pad = 6.0f;

		if (texture && texture->IsLoaded()) {
			ImGui::SetCursorScreenPos(ImVec2(slotMin.x + pad, slotMin.y + (slotH - thumbSize) * 0.5f));
			UI::Image(texture, Size(thumbSize, thumbSize));

			float textX = slotMin.x + pad + thumbSize + 8.0f;
			float textY = slotMin.y + (slotH - ImGui::GetTextLineHeight() * 2.5f) * 0.5f;

			ImGui::SetCursorScreenPos(ImVec2(textX, textY));
			{
				ScopedColor textColor(ImGuiCol_Text, MaterialEditorStyle::TextHeader());
				std::filesystem::path texPath(path);
				UI::Text("%s", texPath.filename().string().c_str());
			}

			ImGui::SetCursorScreenPos(ImVec2(textX, textY + ImGui::GetTextLineHeight() + 2.0f));
			{
				ScopedColor textColor(ImGuiCol_Text, MaterialEditorStyle::TextMuted());
				UI::Text("%dx%d", texture->GetWidth(), texture->GetHeight());
			}

			float btnW = 16.0f;
			float btnH = 16.0f;
			ImGui::SetCursorScreenPos(ImVec2(slotMax.x - btnW - pad - 2.0f, slotMin.y + (slotH - btnH) * 0.5f));
			{
				ScopedColor btnColors({
					{ImGuiCol_Button, MaterialEditorStyle::ButtonDanger()},
					{ImGuiCol_ButtonHovered, MaterialEditorStyle::ButtonDangerHover()},
					{ImGuiCol_ButtonActive, MaterialEditorStyle::ButtonDanger().Darker(0.1f)}
				});
				ScopedStyle btnStyle(ImGuiStyleVar_FrameRounding, 3.0f);
				if (ImGui::SmallButton("X")) {
					onTextureClear();
				}
			}
		}
		else {
			float textY = slotMin.y + (slotH - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::SetCursorScreenPos(ImVec2(slotMin.x + pad + 4.0f, textY));
			{
				ScopedColor textColor(ImGuiCol_Text, MaterialEditorStyle::TextMuted());
				UI::Text("%s - Drop texture here", label.c_str());
			}

			if (slotHovered) {
				drawList->AddRect(
					ImVec2(slotMin.x + 2, slotMin.y + 2),
					ImVec2(slotMax.x - 2, slotMax.y - 2),
					MaterialEditorStyle::AccentPrimary().WithAlpha(0.4f).ToImU32(),
					3.0f, 0, 1.0f);
			}
		}

		// Reserve space and handle drag & drop
		ImGui::SetCursorScreenPos(slotMin);
		ImGui::InvisibleButton("##DropTarget", ImVec2(slotWidth, slotH));

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

		ImGui::SetCursorScreenPos(ImVec2(slotMin.x, slotMax.y + 2.0f));
	}

	// ============================================================================
	// LEGACY COMPATIBILITY
	// ============================================================================

	void MaterialEditorPanel::DrawTextureSlotNew(
		const std::string& label,
		Ref<Texture2D> texture,
		const std::string& path,
		std::function<void(Ref<Texture2D>)> onTextureSet,
		std::function<void()> onTextureClear) {

		DrawTextureSlotCompact(label, texture, path, onTextureSet, onTextureClear);
	}

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

		DrawTextureSlotCompact(label, texture, path,
			[loadFunc](Ref<Texture2D>) {},
			[]() {});
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
		}
		else {
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