#pragma once
#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/Texture.h"

#include <string>

namespace Lunex {
	class PropertiesPanel {
	public:
		PropertiesPanel();
		~PropertiesPanel() = default;

		void OnImGuiRender();
		void SetContext(const Ref<Scene>& context) { m_Context = context; }
		void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

	private:
		void DrawComponents(Entity entity);
		void RenderComponentHeader(const char* name, bool* open, bool removable = true);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;

		// Icons for components
		Ref<Texture2D> m_TransformIcon;
		Ref<Texture2D> m_CameraIcon;
		Ref<Texture2D> m_SpriteIcon;
		Ref<Texture2D> m_CircleIcon;
		Ref<Texture2D> m_PhysicsIcon;
		Ref<Texture2D> m_AddIcon;
		Ref<Texture2D> m_SettingsIcon;
	};
}
