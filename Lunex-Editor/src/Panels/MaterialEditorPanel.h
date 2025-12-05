#pragma once

#include "Core/Core.h"
#include "Renderer/MaterialAsset.h"
#include "Renderer/MaterialPreviewRenderer.h"
#include <filesystem>
#include <functional>

namespace Lunex {

	// MaterialEditorPanel - Editor visual de materiales con preview en tiempo real
	// 
	// Características:
	//   - Preview 3D interactivo con esfera
	//   - Controles PBR (Metallic, Roughness, Specular, etc.)
	//   - Drag & Drop de texturas
	//   - Guardado automático o manual
	//   - Múltiples materiales abiertos en tabs
	class MaterialEditorPanel {
	public:
		MaterialEditorPanel();
		~MaterialEditorPanel() = default;

		// ========== CONTROL DEL PANEL ==========

		// Abrir material para edición
		void OpenMaterial(Ref<MaterialAsset> material);
		void OpenMaterial(const std::filesystem::path& materialPath);

		// Cerrar material actual
		void CloseMaterial();

		// Verificar si hay un material abierto
		bool IsMaterialOpen() const { return m_EditingMaterial != nullptr; }

		// Obtener material en edición
		Ref<MaterialAsset> GetEditingMaterial() const { return m_EditingMaterial; }

		// ========== CALLBACKS ==========
		
		// Called when a material is saved (for hot reloading)
		using MaterialSavedCallback = std::function<void(const std::filesystem::path&)>;
		void SetOnMaterialSavedCallback(MaterialSavedCallback callback) { m_OnMaterialSaved = callback; }

		// ========== RENDERING ==========

		// Renderizar el panel (llamar en OnImGuiRender)
		void OnImGuiRender();

		// Actualizar preview (llamar en OnUpdate)
		void OnUpdate(float deltaTime);

		// ========== CONFIGURACIÓN ==========

		// Auto-guardar cuando se modifican propiedades
		void SetAutoSave(bool enabled) { m_AutoSave = enabled; }
		bool IsAutoSaveEnabled() const { return m_AutoSave; }

		// Tamaño del viewport de preview
		void SetPreviewSize(uint32_t width, uint32_t height);

	private:
		// Material en edición
		Ref<MaterialAsset> m_EditingMaterial;

		// Preview renderer
		Ref<MaterialPreviewRenderer> m_PreviewRenderer;

		// Callbacks
		MaterialSavedCallback m_OnMaterialSaved;

		// UI State
		bool m_IsOpen = false;
		bool m_AutoSave = false;
		bool m_HasUnsavedChanges = false;

		// Preview viewport size
		uint32_t m_PreviewWidth = 512;
		uint32_t m_PreviewHeight = 512;

		// ========== UI RENDERING ==========

		void DrawMenuBar();
		void DrawPreviewViewport();
		void DrawPropertiesPanel();
		void DrawTexturesPanel();

		// Property controls (devuelven true si hubo cambio)
		bool DrawColorProperty(const char* label, glm::vec4& color);
		bool DrawColor3Property(const char* label, glm::vec3& color);
		bool DrawFloatProperty(const char* label, float& value, float min, float max);
		
		// DrawTextureSlot ahora acepta Ref por valor (no por referencia)
		void DrawTextureSlot(const char* label, const char* icon, 
							 Ref<Texture2D> texture, const std::string& path,
							 std::function<void(const std::string&)> loadFunc);

		// ========== HELPERS ==========

		void SaveMaterial();
		void MarkAsModified();
		bool ShowUnsavedChangesDialog();
	};

}
