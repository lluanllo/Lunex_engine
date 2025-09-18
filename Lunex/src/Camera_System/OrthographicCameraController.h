#pragma once

#include "Core/Core.h"

#include "Renderer/OrthographicCamera.h"
#include "Core/Timestep.h"

#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"

namespace Lunex {
	class LUNEX_API OrthographicCameraController {
		public:
			OrthographicCameraController(float aspectRatio);
			
			void OnUpdate(Timestep ts);
			void OnEvent(Event& e);
			
		private:
			bool OnMouseScrolled(MouseScrolledEvent& e);
			bool OnWindowResized(WindowResizeEvent& e);
			
			void Resize(float width, float height);
			
			OrthographicCamera& GetCamera() { return m_Camera; }
			const OrthographicCamera& GetCamera() const { return m_Camera; }
			
			float GetZoomLevel() const { return m_ZoomLevel; }
			
			void SetZoomLevel(float level) { m_ZoomLevel = level; Resize(m_AspectRatio, m_AspectRatio / level); }
			
		private:
			float m_AspectRatio;
			float m_ZoomLevel = 1.0f;
			OrthographicCamera m_Camera;
	};
}