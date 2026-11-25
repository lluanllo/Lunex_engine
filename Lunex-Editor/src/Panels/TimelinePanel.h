#pragma once

/**
 * @file TimelinePanel.h
 * @brief Blender-style playback timeline docked above the Content Browser.
 *
 * Always visible: plays / pauses / scrubs the animation of the selected entity (or the
 * first animated entity of the scene when nothing animated is selected). Keyframes of
 * authored .luanim clips are shown as diamonds. In Edit mode playback uses the
 * Animator's editor preview, in Play mode it drives the runtime state.
 */

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

namespace Lunex {

	struct AnimatorComponent;

	class TimelinePanel {
	public:
		void SetContext(const Ref<Scene>& scene, bool runtime) { m_Scene = scene; m_Runtime = runtime; }
		void SetSelectedEntity(Entity entity) { m_Selected = entity; }

		void OnImGuiRender();

		bool IsOpen() const { return m_Open; }
		void SetOpen(bool open) { m_Open = open; }

	private:
		Entity ResolveTarget();
		void DockAboveContentBrowser();
		float GetClipDuration(Entity entity, AnimatorComponent& animator, std::vector<float>* keyTimes);

		Ref<Scene> m_Scene;
		Entity m_Selected;
		bool m_Runtime = false;
		bool m_Open = true;
		bool m_DockChecked = false;
		float m_FrameRate = 30.0f;
	};

} // namespace Lunex
