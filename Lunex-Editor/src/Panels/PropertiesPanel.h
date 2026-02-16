#pragma once

#include "../UI/LunexUI.h"
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
		
		// Cache management for hot reloading
		void InvalidateMaterialThumbnail(UUID assetID);
		void ClearThumbnailCache();

	private:
		void DrawComponents(Entity entity);
		void DrawEmptyState();
		
		// Component drawing helpers
		void DrawTransformComponent(Entity entity);
		void DrawScriptComponent(Entity entity);
		void DrawCameraComponent(Entity entity);
		void DrawSpriteRendererComponent(Entity entity);
		void DrawCircleRendererComponent(Entity entity);
		void DrawMeshComponent(Entity entity);
		void DrawMaterialComponent(Entity entity);
		void DrawLightComponent(Entity entity);
		
		// 2D Physics
		void DrawRigidbody2DComponent(Entity entity);
		void DrawBoxCollider2DComponent(Entity entity);
		void DrawCircleCollider2DComponent(Entity entity);
		
		// 3D Physics
		void DrawRigidbody3DComponent(Entity entity);
		void DrawBoxCollider3DComponent(Entity entity);
		void DrawSphereCollider3DComponent(Entity entity);
		void DrawCapsuleCollider3DComponent(Entity entity);
		void DrawCylinderCollider3DComponent(Entity entity);
		void DrawConeCollider3DComponent(Entity entity);
		void DrawMeshCollider3DComponent(Entity entity);
		void DrawCharacterController3DComponent(Entity entity);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);
		
		// ===== THUMBNAIL SYSTEM =====
		Ref<Texture2D> GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		
		// Callback
		std::function<void(Ref<MaterialAsset>)> m_OnMaterialEditCallback;
		
		// ===== THUMBNAIL CACHE =====
		std::unordered_map<UUID, Ref<Texture2D>> m_ThumbnailCache;
		Scope<MaterialPreviewRenderer> m_PreviewRenderer;
	};

}
