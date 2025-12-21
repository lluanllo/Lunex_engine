#pragma once

/**
 * @file OrthographicCameraController.h
 * @brief Controller for 2D orthographic camera
 * 
 * AAA Architecture: OrthographicCameraController lives in Scene/Camera/
 * Handles input for 2D camera movement.
 */

#include "Scene/Camera/OrthographicCamera.h"
#include "Scene/Camera/CameraController.h"
#include "Core/Core.h"
#include "Core/Timestep.h"
#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"

namespace Lunex {

	/**
	 * @class OrthographicCameraController
	 * @brief Controls an OrthographicCamera with keyboard/mouse input
	 */
	class OrthographicCameraController {
	public:
		OrthographicCameraController(float aspectRatio, bool rotation = false);
		
		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);
		
		void OnResize(float width, float height);
		
		// ========== CAMERA ACCESS ==========
		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const { return m_Camera; }
		
		// ========== ZOOM ==========
		float GetZoomLevel() const { return m_ZoomLevel; }
		void SetZoomLevel(float level) { m_ZoomLevel = level; }
		
		// ========== VIEW DATA (AAA Architecture) ==========
		ViewData GetViewData() const { return m_Camera.GetViewData(); }
		
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
		float m_CameraTranslationSpeed = 5.0f;
		float m_CameraRotationSpeed = 180.0f;
	};
	
} // namespace Lunex
