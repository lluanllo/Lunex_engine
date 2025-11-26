#pragma once

#include "Renderer/Renderer3D.h"

namespace Lunex {
	class SettingsPanel {
	public:
		SettingsPanel() = default;
		~SettingsPanel() = default;

		void OnImGuiRender();

		bool GetShowPhysicsColliders() const { return m_ShowPhysicsColliders; }
		void SetShowPhysicsColliders(bool show) { m_ShowPhysicsColliders = show; }
		
		// Ray Tracing controls
		bool GetEnableComputeRayTracing() const { return m_EnableComputeRayTracing; }
		void SetEnableComputeRayTracing(bool enable) { m_EnableComputeRayTracing = enable; }

	private:
		bool m_ShowPhysicsColliders = false;
		bool m_EnableComputeRayTracing = false;  // ? AGREGAR
		
		// Ray Tracing UI state
		bool m_RayTracingExpanded = true;
	};
}
