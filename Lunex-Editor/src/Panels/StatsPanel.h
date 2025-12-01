#pragma once

#include "Core/Core.h"
#include "Scene/Entity.h"

namespace Lunex {
	class StatsPanel {
	public:
		StatsPanel() = default;
		~StatsPanel() = default;

		void OnImGuiRender();

		void SetHoveredEntity(Entity entity) { m_HoveredEntity = entity; }
		
		void Toggle() { m_IsOpen = !m_IsOpen; }
		bool IsOpen() const { return m_IsOpen; }
		void SetOpen(bool open) { m_IsOpen = open; }

	private:
		Entity m_HoveredEntity;
		bool m_IsOpen = true;
	};
}
