#pragma once

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/MaterialPreviewRenderer.h"
#include <functional>
#include <unordered_map>

namespace Lunex {
	class MaterialAsset;

	class PropertiesPanel {
	public:
		PropertiesPanel() = default;
		PropertiesPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);
		void SetSelectedEntity(Entity entity);

		void OnImGuiRender();
		
		// Callback for opening material editor
		void SetOnMaterialEditCallback(const std::function<void(Ref<MaterialAsset>)>& callback) {
			m_OnMaterialEditCallback = callback;
		}

	private:
		void DrawComponents(Entity entity);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);
		
		// ===== THUMBNAIL SYSTEM =====
		uint32_t GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset);
		void InvalidateThumbnail(UUID assetID);
		void ClearThumbnailCache();

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		
		// Callback
		std::function<void(Ref<MaterialAsset>)> m_OnMaterialEditCallback;
		
		// ===== THUMBNAIL CACHE =====
		// Maps MaterialAsset UUID -> OpenGL Texture ID
		std::unordered_map<UUID, uint32_t> m_ThumbnailCache;
		Scope<MaterialPreviewRenderer> m_PreviewRenderer;
	};

}
