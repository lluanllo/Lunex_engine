#include "stpch.h"
#include "OrthographicCameraController.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Input/InputManager.h"

namespace Lunex {

	OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool rotation)
		: m_AspectRatio(aspectRatio)
		, m_Camera(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel)
		, m_Rotation(rotation)
	{
	}
	
	void OrthographicCameraController::OnUpdate(Timestep ts) {
		LNX_PROFILE_FUNCTION();
		
		auto& inputMgr = InputManager::Get();
		
		if (inputMgr.IsActionPressed("Camera.MoveLeft")) {
			m_CameraPosition.x -= cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
			m_CameraPosition.y -= sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		}
		else if (inputMgr.IsActionPressed("Camera.MoveRight")) {
			m_CameraPosition.x += cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
			m_CameraPosition.y += sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		}
		
		if (inputMgr.IsActionPressed("Camera.MoveForward")) {
			m_CameraPosition.x += -sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
			m_CameraPosition.y += cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		}
		else if (inputMgr.IsActionPressed("Camera.MoveBackward")) {
			m_CameraPosition.x -= -sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
			m_CameraPosition.y -= cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		}
		
		if (m_Rotation) {
			if (inputMgr.IsActionPressed("Camera.MoveDown"))
				m_CameraRotation += m_CameraRotationSpeed * ts;
			if (inputMgr.IsActionPressed("Camera.MoveUp"))
				m_CameraRotation -= m_CameraRotationSpeed * ts;
			
			if (m_CameraRotation > 180.0f)
				m_CameraRotation -= 360.0f;
			else if (m_CameraRotation <= -180.0f)
				m_CameraRotation += 360.0f;
			
			m_Camera.SetRotation(m_CameraRotation);
		}
		
		m_Camera.SetPosition(m_CameraPosition);
		m_CameraTranslationSpeed = m_ZoomLevel;
	}
	
	void OrthographicCameraController::OnEvent(Event& e) {
		LNX_PROFILE_FUNCTION();
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(LNX_BIND_EVENT_FN(OrthographicCameraController::OnMouseScrolled));
		dispatcher.Dispatch<WindowResizeEvent>(LNX_BIND_EVENT_FN(OrthographicCameraController::OnWindowResized));
	}
	
	void OrthographicCameraController::OnResize(float width, float height) {
		m_AspectRatio = width / height;
		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
	}
	
	bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e) {
		LNX_PROFILE_FUNCTION();
		m_ZoomLevel -= e.GetYOffset() * 0.5f;
		m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);
		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
		return false;
	}
	
	bool OrthographicCameraController::OnWindowResized(WindowResizeEvent& e) {
		LNX_PROFILE_FUNCTION();
		m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
		return false;
	}
	
} // namespace Lunex
