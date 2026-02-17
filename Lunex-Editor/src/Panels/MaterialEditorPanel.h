#pragma once

/**
 * @file MaterialEditorPanel.h
 * @brief AAA-Quality Material Editor Panel
 *
 * Features:
 * - Real-time PBR material preview
 * - Clean, professional UI using Lunex UI Framework
 * - Collapsible sections with persistent state
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

#include <imgui.h>
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

		// UI state
		bool m_IsOpen = false;
		bool m_AutoSave = false;
		bool m_HasUnsavedChanges = false;

		// Section collapse state (persistent during session)
		bool m_SectionBaseColor = true;
		bool m_SectionPBR = true;
		bool m_SectionSurface = false;
		bool m_SectionTextures = true;
		bool m_SectionLayered = false;
		bool m_SectionEmission = false;
		bool m_SectionHeight = false;
		bool m_SectionDetail = false;
		bool m_SectionInfo = false;

		// Preview state
		uint32_t m_PreviewWidth = 512;
		uint32_t m_PreviewHeight = 512;

		// ========== UI DRAWING ==========

		void DrawMainLayout();
		void DrawMenuBar();
		void DrawPreviewViewport();
		void DrawPropertiesPanel();

		// Section drawing - reorganized for clarity
		void DrawBaseColorSection();
		void DrawPBRPropertiesSection();
		void DrawSurfaceSettingsSection();
		void DrawTextureMapsSection();
		void DrawLayeredTextureSection();
		void DrawEmissionSection();
		void DrawHeightMapSection();
		void DrawDetailMapSection();
		void DrawMaterialInfoSection();

		// Collapsible section header helper
		// accentColor: optional ImVec4 accent bar color (pass nullptr for no accent)
		bool DrawSectionHeader(const char* label, bool& isOpen, const ImVec4* accentColor = nullptr);

		// Compact texture slot
		void DrawTextureSlotCompact(
			const std::string& label,
			Ref<Texture2D> texture,
			const std::string& path,
			std::function<void(Ref<Texture2D>)> onTextureSet,
			std::function<void()> onTextureClear);

		// New texture slot with callbacks (kept for compatibility)
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
