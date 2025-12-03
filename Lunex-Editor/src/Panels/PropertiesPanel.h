#pragma once

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include <functional>

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

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		
		// Callback
		std::function<void(Ref<MaterialAsset>)> m_OnMaterialEditCallback;
	};

}
