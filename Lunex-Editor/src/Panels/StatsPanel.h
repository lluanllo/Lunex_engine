#pragma once

#include "Core/Core.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

namespace Lunex {

	/**
	 * Engine performance at a glance: frame time graph, where each millisecond goes
	 * (CPU and GPU per pass / per scene system), rendering counters, memory (RAM, VRAM,
	 * textures) and scene contents.
	 */
	class StatsPanel {
	public:
		StatsPanel() = default;
		~StatsPanel() = default;

		void OnImGuiRender();

		void SetHoveredEntity(Entity entity) { m_HoveredEntity = entity; }
		void SetContext(const Ref<Scene>& scene) { m_Scene = scene; }

		void Toggle() { m_IsOpen = !m_IsOpen; }
		bool IsOpen() const { return m_IsOpen; }
		void SetOpen(bool open) { m_IsOpen = open; }

	private:
		void DrawFrame();
		void DrawTimings();
		void DrawRendering();
		void DrawMemory();
		void DrawScene();

		Entity m_HoveredEntity;
		Ref<Scene> m_Scene;
		bool m_IsOpen = true;
		bool m_SortByCost = false;
	};
}
