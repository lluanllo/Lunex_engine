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
		void SetRayTracingManager(class RayTracingManager* rtManager) { m_RayTracingManager = rtManager; }

	private:
		Entity m_HoveredEntity;
		class RayTracingManager* m_RayTracingManager = nullptr;
	};
}
