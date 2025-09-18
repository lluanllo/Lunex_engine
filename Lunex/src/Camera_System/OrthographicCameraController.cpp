#include "stpch.h"

#include "OrthographicCameraController.h"

namespace Lunex{
	OrthographicCameraController::OrthographicCameraController(float aspectRatio)
		: m_AspectRatio(aspectRatio), m_Camera(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel)
	{
		
	}
	
	bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e) {
		m_ZoomLevel -= e.GetYOffset() * 0.25f;
		m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);
		Resize(m_AspectRatio, m_AspectRatio / m_ZoomLevel);
		return false;
	}
}