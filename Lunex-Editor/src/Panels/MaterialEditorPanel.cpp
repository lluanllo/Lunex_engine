#include "stpch.h"
#include "MaterialEditorPanel.h"
#include "ContentBrowserPanel.h" // Para ContentBrowserPayload
#include "Renderer/RenderCommand.h"
#include "Renderer/MaterialRegistry.h"
#include "Log/Log.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	// UI Constants
	namespace UIStyle {
		constexpr float SECTION_SPACING = 8.0f;
		constexpr float COLUMN_WIDTH = 120.0f;
		
		const ImVec4 COLOR_HEADER = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		const ImVec4 COLOR_SUBHEADER = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
		const ImVec4 COLOR_HINT = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		const ImVec4 COLOR_ACCENT = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
		const ImVec4 COLOR_SUCCESS = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
		const ImVec4 COLOR_WARNING = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
		const ImVec4 COLOR_DANGER = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
		const ImVec4 COLOR_BG_DARK = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
	}

	MaterialEditorPanel::MaterialEditorPanel() {
		m_PreviewRenderer = CreateRef<MaterialPreviewRenderer>();
		m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
	}

	// ========== CONTROL DEL PANEL ==========

	void MaterialEditorPanel::OpenMaterial(Ref<MaterialAsset> material) {
		if (!material) {
			LNX_LOG_ERROR("MaterialEditorPanel::OpenMaterial - Material is null");
			return;
		}

		// Si hay cambios sin guardar, preguntar
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

	// ========== RENDERING ==========

	void MaterialEditorPanel::OnImGuiRender() {
		if (!m_IsOpen || !m_EditingMaterial) {
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
		
		std::string windowTitle = "Material Editor - " + m_EditingMaterial->GetName();
		if (m_HasUnsavedChanges) {
			windowTitle += "*";
		}

		if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
			DrawMenuBar();

			// Layout: Preview a la izquierda, Properties a la derecha
			ImGui::Columns(2, nullptr, true);
			ImGui::SetColumnWidth(0, 600);

			// Preview Viewport
			DrawPreviewViewport();

			ImGui::NextColumn();

			// Properties Panel
			ImGui::BeginChild("PropertiesPanel", ImVec2(0, 0), false);
			DrawPropertiesPanel();
			DrawTexturesPanel();
			ImGui::EndChild();

			ImGui::Columns(1);
		}
		ImGui::End();

		// Si el usuario cerró la ventana
		if (!m_IsOpen && m_EditingMaterial) {
			CloseMaterial();
		}
	}

	void MaterialEditorPanel::OnUpdate(float deltaTime) {
		if (m_PreviewRenderer && m_EditingMaterial) {
			m_PreviewRenderer->Update(deltaTime);
			m_PreviewRenderer->RenderPreview(m_EditingMaterial);
		}
	}

	void MaterialEditorPanel::SetPreviewSize(uint32_t width, uint32_t height) {
		m_PreviewWidth = width;
		m_PreviewHeight = height;
		if (m_PreviewRenderer) {
			m_PreviewRenderer->SetResolution(width, height);
		}
	}

	// ========== UI RENDERING ==========

	void MaterialEditorPanel::DrawMenuBar() {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					SaveMaterial();
				}

				if (ImGui::MenuItem("Save As...")) {
					// TODO: Abrir diálogo de guardado
					LNX_LOG_INFO("Save As not implemented yet");
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Revert")) {
					if (m_EditingMaterial) {
						// Recargar desde disco
						MaterialRegistry::Get().ReloadMaterial(m_EditingMaterial->GetID());
						m_HasUnsavedChanges = false;
					}
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Close", "Ctrl+W")) {
					CloseMaterial();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View")) {
				bool autoRotate = m_PreviewRenderer->GetAutoRotate();
				if (ImGui::Checkbox("Auto Rotate", &autoRotate)) {
					m_PreviewRenderer->SetAutoRotate(autoRotate);
				}

				ImGui::Separator();

				if (ImGui::BeginMenu("Preview Model")) {
					if (ImGui::MenuItem("Sphere")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateSphere());
					}
					if (ImGui::MenuItem("Cube")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateCube());
					}
					if (ImGui::MenuItem("Plane")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreatePlane());
					}
					if (ImGui::MenuItem("Cylinder")) {
						m_PreviewRenderer->SetPreviewModel(Model::CreateCylinder());
					}
					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			// Status info a la derecha
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (m_HasUnsavedChanges) {
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_WARNING);
				ImGui::Text("Unsaved Changes");
				ImGui::PopStyleColor();
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUCCESS);
				ImGui::Text("Saved");
				ImGui::PopStyleColor();
			}

			ImGui::EndMenuBar();
		}
	}

	void MaterialEditorPanel::DrawPreviewViewport() {
		ImGui::BeginChild("Preview", ImVec2(0, 0), true);

		ImGui::Text("Preview");
		ImGui::Separator();

		// Obtener el tamaño disponible
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		// Actualizar resolución del preview renderer si cambió
		if (m_PreviewRenderer && (viewportSize.x != m_PreviewWidth || viewportSize.y != m_PreviewHeight)) {
			if (viewportSize.x > 0 && viewportSize.y > 0) {
				m_PreviewWidth = (uint32_t)viewportSize.x;
				m_PreviewHeight = (uint32_t)viewportSize.y;
				m_PreviewRenderer->SetResolution(m_PreviewWidth, m_PreviewHeight);
			}
		}

		// Renderizar preview
		if (m_PreviewRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
			uint32_t textureID = m_PreviewRenderer->GetPreviewTextureID();
			if (textureID > 0) {
				ImGui::Image(
					(void*)(intptr_t)textureID,
					viewportSize,
					ImVec2(0, 1), ImVec2(1, 0)
				);
			} else {
				// Mostrar mensaje si no hay textura
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::Text("Preview not available");
				ImGui::PopStyleColor();
			}
		}

		ImGui::EndChild();
	}

	void MaterialEditorPanel::DrawPropertiesPanel() {
		if (!m_EditingMaterial) return;

		ImGui::Text("PBR Properties");
		ImGui::Separator();
		ImGui::Spacing();

		// Albedo
		glm::vec4 albedo = m_EditingMaterial->GetAlbedo();
		if (DrawColorProperty("Albedo", albedo)) {
			m_EditingMaterial->SetAlbedo(albedo);
			MarkAsModified();
		}

		ImGui::Spacing();

		// Metallic
		float metallic = m_EditingMaterial->GetMetallic();
		if (DrawFloatProperty("Metallic", metallic, 0.0f, 1.0f)) {
			m_EditingMaterial->SetMetallic(metallic);
			MarkAsModified();
		}

		// Roughness
		float roughness = m_EditingMaterial->GetRoughness();
		if (DrawFloatProperty("Roughness", roughness, 0.0f, 1.0f)) {
			m_EditingMaterial->SetRoughness(roughness);
			MarkAsModified();
		}

		// Specular
		float specular = m_EditingMaterial->GetSpecular();
		if (DrawFloatProperty("Specular", specular, 0.0f, 1.0f)) {
			m_EditingMaterial->SetSpecular(specular);
			MarkAsModified();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Emission
		ImGui::Text("Emission");
		ImGui::Spacing();

		glm::vec3 emissionColor = m_EditingMaterial->GetEmissionColor();
		if (DrawColor3Property("Color", emissionColor)) {
			m_EditingMaterial->SetEmissionColor(emissionColor);
			MarkAsModified();
		}

		float emissionIntensity = m_EditingMaterial->GetEmissionIntensity();
		if (DrawFloatProperty("Intensity", emissionIntensity, 0.0f, 100.0f)) {
			m_EditingMaterial->SetEmissionIntensity(emissionIntensity);
			MarkAsModified();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Normal Intensity
		float normalIntensity = m_EditingMaterial->GetNormalIntensity();
		if (DrawFloatProperty("Normal Intensity", normalIntensity, 0.0f, 2.0f)) {
			m_EditingMaterial->SetNormalIntensity(normalIntensity);
			MarkAsModified();
		}
	}

	void MaterialEditorPanel::DrawTexturesPanel() {
		if (!m_EditingMaterial) return;

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Texture Maps");
		ImGui::Separator();
		ImGui::Spacing();

		// Albedo
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetAlbedoMap();
			std::string path = m_EditingMaterial->GetAlbedoPath();
			DrawTextureSlot("Albedo", "🎨", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetAlbedoMap(texture);
						MarkAsModified();
					}
				});
		}

		// Normal
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetNormalMap();
			std::string path = m_EditingMaterial->GetNormalPath();
			DrawTextureSlot("Normal", "🧭", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetNormalMap(texture);
						MarkAsModified();
					}
				});
		}

		// Metallic with multiplier
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetMetallicMap();
			std::string path = m_EditingMaterial->GetMetallicPath();
			DrawTextureSlot("Metallic", "⚙️", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetMetallicMap(texture);
						MarkAsModified();
					}
				});
			
			if (m_EditingMaterial->HasMetallicMap()) {
				float mult = m_EditingMaterial->GetMetallicMultiplier();
				if (DrawFloatProperty("  Multiplier", mult, 0.0f, 2.0f)) {
					m_EditingMaterial->SetMetallicMultiplier(mult);
					MarkAsModified();
				}
			}
		}

		// Roughness with multiplier
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetRoughnessMap();
			std::string path = m_EditingMaterial->GetRoughnessPath();
			DrawTextureSlot("Roughness", "🔧", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetRoughnessMap(texture);
						MarkAsModified();
					}
				});
			
			if (m_EditingMaterial->HasRoughnessMap()) {
				float mult = m_EditingMaterial->GetRoughnessMultiplier();
				if (DrawFloatProperty("  Multiplier", mult, 0.0f, 2.0f)) {
					m_EditingMaterial->SetRoughnessMultiplier(mult);
					MarkAsModified();
				}
			}
		}

		// Specular with multiplier
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetSpecularMap();
			std::string path = m_EditingMaterial->GetSpecularPath();
			DrawTextureSlot("Specular", "💎", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetSpecularMap(texture);
						MarkAsModified();
					}
				});
			
			if (m_EditingMaterial->HasSpecularMap()) {
				float mult = m_EditingMaterial->GetSpecularMultiplier();
				if (DrawFloatProperty("  Multiplier", mult, 0.0f, 2.0f)) {
					m_EditingMaterial->SetSpecularMultiplier(mult);
					MarkAsModified();
				}
			}
		}

		// Emission
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetEmissionMap();
			std::string path = m_EditingMaterial->GetEmissionPath();
			DrawTextureSlot("Emission", "💡", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetEmissionMap(texture);
						MarkAsModified();
					}
				});
		}

		// AO with multiplier
		{
			Ref<Texture2D> tex = m_EditingMaterial->GetAOMap();
			std::string path = m_EditingMaterial->GetAOPath();
			DrawTextureSlot("Ambient Occlusion", "🌑", tex, path,
				[this](const std::string& loadPath) {
					auto texture = Texture2D::Create(loadPath);
					if (texture && texture->IsLoaded()) {
						m_EditingMaterial->SetAOMap(texture);
						MarkAsModified();
					}
				});
			
			if (m_EditingMaterial->HasAOMap()) {
				float mult = m_EditingMaterial->GetAOMultiplier();
				if (DrawFloatProperty("  Multiplier", mult, 0.0f, 2.0f)) {
					m_EditingMaterial->SetAOMultiplier(mult);
					MarkAsModified();
				}
			}
		}
	}

	// ========== PROPERTY CONTROLS ==========

	bool MaterialEditorPanel::DrawColorProperty(const char* label, glm::vec4& color) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		ImGui::Text("%s", label);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit4(("##" + std::string(label)).c_str(), glm::value_ptr(color), 
			ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::Columns(1);
		return changed;
	}

	bool MaterialEditorPanel::DrawColor3Property(const char* label, glm::vec3& color) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		ImGui::Text("%s", label);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit3(("##" + std::string(label)).c_str(), glm::value_ptr(color), 
			ImGuiColorEditFlags_NoLabel);
		ImGui::Columns(1);
		return changed;
	}

	bool MaterialEditorPanel::DrawFloatProperty(const char* label, float& value, float min, float max) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		ImGui::Text("%s", label);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, UIStyle::COLOR_ACCENT);
		bool changed = ImGui::SliderFloat(("##" + std::string(label)).c_str(), &value, min, max, "%.2f");
		ImGui::PopStyleColor();
		ImGui::Columns(1);
		return changed;
	}

	void MaterialEditorPanel::DrawTextureSlot(const char* label, const char* icon,
		Ref<Texture2D> texture, const std::string& path,
		std::function<void(const std::string&)> loadFunc) {
		
		ImGui::PushID(label);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, UIStyle::COLOR_BG_DARK);
		ImGui::BeginChild(("##Tex" + std::string(label)).c_str(), ImVec2(-1, 80), true);

		// Icon and label
		ImGui::Text("%s %s", icon, label);
		
		// Remove button if texture exists
		if (texture) {
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
			ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_DANGER);
			if (ImGui::Button("Remove", ImVec2(60, 0))) {
				// Clear texture by setting nullptr
				if (std::string(label) == "Albedo") m_EditingMaterial->SetAlbedoMap(nullptr);
				else if (std::string(label) == "Normal") m_EditingMaterial->SetNormalMap(nullptr);
				else if (std::string(label) == "Metallic") m_EditingMaterial->SetMetallicMap(nullptr);
				else if (std::string(label) == "Roughness") m_EditingMaterial->SetRoughnessMap(nullptr);
				else if (std::string(label) == "Specular") m_EditingMaterial->SetSpecularMap(nullptr);
				else if (std::string(label) == "Emission") m_EditingMaterial->SetEmissionMap(nullptr);
				else if (std::string(label) == "Ambient Occlusion") m_EditingMaterial->SetAOMap(nullptr);
				MarkAsModified();
			}
			ImGui::PopStyleColor();
		}

		ImGui::Separator();

		// Thumbnail or drop zone
		if (texture && texture->IsLoaded()) {
			ImGui::Image(
				(void*)(intptr_t)texture->GetRendererID(),
				ImVec2(50, 50),
				ImVec2(0, 1), ImVec2(1, 0)
			);
			ImGui::SameLine();
			
			std::filesystem::path texPath(path);
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
			ImGui::TextWrapped("%s", texPath.filename().string().c_str());
			ImGui::PopStyleColor();
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
			ImGui::Text("Drop texture here");
			ImGui::PopStyleColor();
		}

		// Drag and drop
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
					loadFunc(data->FilePath);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopID();
		ImGui::Spacing();
	}

	// ========== HELPERS ==========

	void MaterialEditorPanel::SaveMaterial() {
		if (!m_EditingMaterial) return;

		if (m_EditingMaterial->SaveToFile()) {
			m_HasUnsavedChanges = false;
			LNX_LOG_INFO("Material saved: {0}", m_EditingMaterial->GetName());
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
		// TODO: Implementar diálogo modal de ImGui
		// Por ahora, auto-guardar
		if (m_HasUnsavedChanges) {
			SaveMaterial();
		}
		return true;
	}

}
