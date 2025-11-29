#pragma once

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

namespace Lunex {

	class PropertiesPanel {
	public:
		PropertiesPanel() = default;
		PropertiesPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);
		void SetSelectedEntity(Entity entity);

		void OnImGuiRender();

	private:
		void DrawComponents(Entity entity);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
	};

}
