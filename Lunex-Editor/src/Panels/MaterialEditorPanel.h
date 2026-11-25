#pragma once

/**
 * @file MaterialEditorPanel.h
 * @brief AAA-Quality Material Editor Panel with Dockable Sub-Panels
 * 
 * Features:
 * - Real-time PBR material preview (dockable)
 * - Texture Maps panel for all texture assignments (dockable)
 * - PBR Properties panel for base material values (dockable)
 * - Advanced Options panel for layered/channel-packed textures (dockable)
 * - 100% Lunex UI Framework (no raw ImGui)
 */

#include "Core/Core.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Resources/Render/MaterialInstance.h"
#include "Renderer/MaterialPreviewRenderer.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lunex {

	/**
	 * @class MaterialEditorPanel
	 * @brief Visual material editor with 4 dockable sub-panels
	 * 
	 * Layout (default):
	 * ?????????????????????????????????????
	 * ?               ?  PBR Properties   ?
	 * ?   Preview     ?????????????????????
	 * ?               ?  Texture Maps     ?
	 * ?               ?????????????????????
	 * ?               ? Advanced Options  ?
	 * ?????????????????????????????????????
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

		// Node graph materials are edited in their own window (MaterialGraphPanel)
		using OpenNodeEditorCallback = std::function<void(Ref<MaterialAsset>)>;
		void SetOnOpenNodeEditorCallback(OpenNodeEditorCallback callback) { m_OnOpenNodeEditor = std::move(callback); }

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

		// Preview state
		uint32_t m_PreviewWidth = 512;
		uint32_t m_PreviewHeight = 512;

		// ========== HOST WINDOW ==========

		void DrawHostWindow();
		void DrawMenuBar();
		void SetupDockspace(unsigned int dockspaceID);

		// ========== SUB-PANELS (Dockable) ==========

		void DrawPreviewPanel();
		void DrawTextureMapsPanel();
		void DrawPBRPropertiesPanel();
		void DrawAdvancedOptionsPanel();

		// ========== NODE GRAPH MODE ==========
		// Properties mode edits the PBR values directly; Node Graph mode makes the graph
		// the source of truth (compiled to GLSL) and opens the node editor window.
		void SetNodeMode(bool enabled);
		void DrawUVSection();

		OpenNodeEditorCallback m_OnOpenNodeEditor;

		// ========== SECTION DRAWING ==========

		void DrawPBRSection();
		void DrawEmissionSection();
		void DrawSubsurfaceSection();
		void DrawTextureSlotsSection();
		void DrawDetailNormalsSection();
		void DrawLayeredTextureSection();
		void DrawChannelPackingSection();

		// ========== TEXTURE SLOT ==========

		void DrawTextureSlotNew(
			const std::string& label,
			Ref<Texture2D> texture,
			const std::string& path,
			TextureColorSpace colorSpace,
			std::function<void(Ref<Texture2D>)> onTextureSet,
			std::function<void()> onTextureClear,
			std::function<void(TextureColorSpace)> onColorSpaceChanged);

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
