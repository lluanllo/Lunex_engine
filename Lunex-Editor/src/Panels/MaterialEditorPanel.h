#pragma once

/**
 * @file MaterialEditorPanel.h
 * @brief AAA-Quality Material Editor Panel
 * 
 * Features:
 * - Real-time PBR material preview
 * - Clean, professional UI using Lunex UI Framework
 * - Drag & drop texture support
 * - Layered (ORM) texture support
 * - Height/Parallax mapping
 * - Detail normal maps
 * - Surface settings (alpha mode, two-sided, UV tiling)
 * - Hot-reload support
 */

#include "Core/Core.h"

// AAA Architecture - Material system
#include "Assets/Materials/MaterialAsset.h"
#include "Resources/Render/MaterialInstance.h"
#include "Renderer/MaterialPreviewRenderer.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	/**
	 * @class MaterialEditorPanel
	 * @brief Visual material editor with real-time preview
	 */
	class MaterialEditorPanel {
	public:
		MaterialEditorPanel();
		~MaterialEditorPanel() = default;

		// ========== PANEL CONTROL ==========

		void OpenMaterial(Ref<MaterialAsset> material);
		void OpenMaterial(const std::filesystem::path& materialPath);
		void CloseMaterial();

		bool IsMaterialOpen() const { return m_EditingMaterial != nullptr; }
		Ref<MaterialAsset> GetEditingMaterial() const { return m_EditingMaterial; }

		// ========== CALLBACKS ==========
		
		using MaterialSavedCallback = std::function<void(const std::filesystem::path&)>;
		void SetOnMaterialSavedCallback(MaterialSavedCallback callback) { m_OnMaterialSaved = callback; }

		using OpenShaderEditorCallback = std::function<void(Ref<MaterialAsset>)>;
		void SetOnOpenShaderEditorCallback(OpenShaderEditorCallback callback) { m_OnOpenShaderEditor = callback; }

		// ========== RENDERING ==========

		void OnImGuiRender();
		void OnUpdate(float deltaTime);

		// ========== CONFIGURATION ==========

		void SetAutoSave(bool enabled) { m_AutoSave = enabled; }
		bool IsAutoSaveEnabled() const { return m_AutoSave; }
		void SetPreviewSize(uint32_t width, uint32_t height);

	private:
		// Material state
		Ref<MaterialAsset> m_EditingMaterial;
		Ref<MaterialPreviewRenderer> m_PreviewRenderer;
		MaterialSavedCallback m_OnMaterialSaved;
		OpenShaderEditorCallback m_OnOpenShaderEditor;

		// UI state
		bool m_IsOpen = false;
		bool m_AutoSave = false;
		bool m_HasUnsavedChanges = false;

		// Preview state
		uint32_t m_PreviewWidth = 512;
		uint32_t m_PreviewHeight = 512;

		// ========== UI DRAWING ==========

		void DrawMainLayout();
		void DrawMenuBar();
		void DrawPreviewViewport();
		void DrawPropertiesPanel();
		
		// Section drawing
		void DrawPBRPropertiesSection();
		void DrawEmissionSection();
		void DrawSurfaceSettingsSection();
		void DrawTextureMapsSection();
		void DrawLayeredTextureSection();
		void DrawHeightMapSection();
		void DrawDetailMapSection();
		void DrawMaterialInfoSection();
		
		// New texture slot with callbacks
		void DrawTextureSlotNew(
			const std::string& label,
			Ref<Texture2D> texture,
			const std::string& path,
			std::function<void(Ref<Texture2D>)> onTextureSet,
			std::function<void()> onTextureClear);

		// ========== LEGACY API (backwards compatibility) ==========

		bool DrawColorProperty(const char* label, glm::vec4& color);
		bool DrawColor3Property(const char* label, glm::vec3& color);
		bool DrawFloatProperty(const char* label, float& value, float min, float max);
		void DrawTextureSlot(const char* label, const char* icon, 
							 Ref<Texture2D> texture, const std::string& path,
							 std::function<void(const std::string&)> loadFunc);

		// ========== HELPERS ==========

		void SaveMaterial();
		void MarkAsModified();
		bool ShowUnsavedChangesDialog();
	};

} // namespace Lunex
