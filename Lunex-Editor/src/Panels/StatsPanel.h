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

	private:
		Entity m_HoveredEntity;
	};
}
