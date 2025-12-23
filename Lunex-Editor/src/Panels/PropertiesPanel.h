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
		
		// Callback for opening animation editor
		void SetOnAnimationEditCallback(const std::function<void(Entity)>& callback) {
			m_OnAnimationEditCallback = callback;
		}
		
		// Cache management for hot reloading
		void InvalidateMaterialThumbnail(UUID assetID);
		void ClearThumbnailCache();

	private:
		void DrawComponents(Entity entity);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);
		
		// ===== THUMBNAIL SYSTEM =====
		Ref<Texture2D> GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		
		// Callbacks
		std::function<void(Ref<MaterialAsset>)> m_OnMaterialEditCallback;
		std::function<void(Entity)> m_OnAnimationEditCallback;
		
		// ===== THUMBNAIL CACHE =====
		// Maps MaterialAsset UUID -> Standalone Texture
		std::unordered_map<UUID, Ref<Texture2D>> m_ThumbnailCache;
		Scope<MaterialPreviewRenderer> m_PreviewRenderer;
	};

}
