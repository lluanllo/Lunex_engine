#pragma once

#include "Core/Core.h"

#include "Renderer/OrthographicCamera.h"
#include "Core/Timestep.h"

#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"

namespace Lunex {
	class LUNEX_API OrthographicCameraController {
		public:
			OrthographicCameraController(float aspectRatio, bool rotation = false);
			
			void OnUpdate(Timestep ts);
			void OnEvent(Event& e);
			
		private:
			bool OnMouseScrolled(MouseScrolledEvent& e);
			bool OnWindowResized(WindowResizeEvent& e);
			
		private:
			float m_AspectRatio;
			float m_ZoomLevel = 1.0f;
			OrthographicCamera m_Camera;
			
			bool m_Rotation;
			
			glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
			float m_CameraRotation = 0.0f;
			float m_CameraTranslationSpeed = 5.0f, m_CameraRotationSpeed = 1.0f;
	};
}