#pragma once

#include "Core/Core.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

namespace Lunex {
	class SceneHierarchyPanel {
		public:
			SceneHierarchyPanel() = default;
			SceneHierarchyPanel(const Ref<Scene>& scene);
			~SceneHierarchyPanel() = default;
			
			void SetContext(const Ref<Scene>& scene);
			
			void OnImGuiRender();
		private:
			void DrawEntityNode(Entity entity);
			void DrawComponents(Entity entity);
		private:
			Ref<Scene> m_Context;
			Entity m_SelectionContext;
	};
}