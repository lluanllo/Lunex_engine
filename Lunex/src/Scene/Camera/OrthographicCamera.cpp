#include "stpch.h"
#include "OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {
	
	OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
		: m_ProjectionMatrix(glm::ortho(left, right, bottom, top, -1.0f, 1.0f))
		, m_ViewMatrix(1.0f)
		, m_Left(left), m_Right(right), m_Bottom(bottom), m_Top(top)
	{
		LNX_PROFILE_FUNCTION();
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	
	void OrthographicCamera::SetProjection(float left, float right, float bottom, float top) {
		LNX_PROFILE_FUNCTION();
		m_Left = left;
		m_Right = right;
		m_Bottom = bottom;
		m_Top = top;
		m_ProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	
	void OrthographicCamera::RecalculateViewMatrix() {
		LNX_PROFILE_FUNCTION();
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));
		
		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	
	ViewData OrthographicCamera::GetViewData() const {
		ViewData data;
		data.ViewMatrix = m_ViewMatrix;
		data.ProjectionMatrix = m_ProjectionMatrix;
		data.CameraPosition = m_Position;
		data.CameraDirection = glm::vec3(0.0f, 0.0f, -1.0f);  // 2D camera looks down -Z
		data.CameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		data.CameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
		data.NearPlane = -1.0f;
		data.FarPlane = 1.0f;
		data.FieldOfView = 0.0f;  // Orthographic has no FOV
		data.AspectRatio = (m_Right - m_Left) / (m_Top - m_Bottom);
		data.ComputeDerivedMatrices();
		return data;
	}
	
} // namespace Lunex
