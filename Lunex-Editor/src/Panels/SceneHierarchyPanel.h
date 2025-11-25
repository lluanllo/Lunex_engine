#pragma once

#include "Core/Core.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/Texture.h"

namespace Lunex {
	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);
		~SceneHierarchyPanel() = default;

		void SetContext(const Ref<Scene>& scene);

		void OnImGuiRender();

		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);
		
	private:
		void DrawEntityNode(Entity entity);
		void RenderTopBar();
		
		template<typename T>
		void CreateEntityWithComponent(const std::string& name) {
			Entity entity = m_Context->CreateEntity(name);
			entity.AddComponent<T>();
			m_SelectionContext = entity;
			LNX_LOG_INFO("Created entity: {0}", name);
		}

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
		
		// Iconos para la jerarquía
		Ref<Texture2D> m_CameraIcon;
		Ref<Texture2D> m_EntityIcon;
		
		// Para colores alternos
		int m_EntityIndexCounter = 0;
		
		// Search filter
		char m_SearchFilter[256] = "";
	};
}