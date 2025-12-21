#include "stpch.h"
#include "SceneCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	SceneCamera::SceneCamera() {
		RecalculateProjection();
	}
	
	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip) {
		m_ProjectionType = ProjectionType::Orthographic;
		m_OrthographicSize = size;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;
		RecalculateProjection();
	}
	
	void SceneCamera::SetPerspective(float verticalFOV, float nearClip, float farClip) {
		m_ProjectionType = ProjectionType::Perspective;
		m_PerspectiveFOV = verticalFOV;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;
		RecalculateProjection();
	}
	
	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height) {
		LNX_CORE_ASSERT(width > 0 && height > 0);
		m_AspectRatio = (float)width / (float)height;
		RecalculateProjection();
	}
	
	void SceneCamera::RecalculateProjection() {
		if (m_ProjectionType == ProjectionType::Perspective) {
			m_Projection = glm::perspective(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
		}
		else {
			float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoBottom = -m_OrthographicSize * 0.5f;
			float orthoTop = m_OrthographicSize * 0.5f;
			
			m_Projection = glm::ortho(orthoLeft, orthoRight,
				orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);
		}
	}
	
	ViewData SceneCamera::GetViewData(const glm::mat4& transform) const {
		ViewData data;
		data.ViewMatrix = glm::inverse(transform);
		data.ProjectionMatrix = m_Projection;
		
		// Extract position and directions from transform
		data.CameraPosition = glm::vec3(transform[3]);
		data.CameraDirection = -glm::normalize(glm::vec3(transform[2]));
		data.CameraUp = glm::normalize(glm::vec3(transform[1]));
		data.CameraRight = glm::normalize(glm::vec3(transform[0]));
		
		// Set clip planes based on projection type
		if (m_ProjectionType == ProjectionType::Perspective) {
			data.NearPlane = m_PerspectiveNear;
			data.FarPlane = m_PerspectiveFar;
			data.FieldOfView = glm::degrees(m_PerspectiveFOV);
		} else {
			data.NearPlane = m_OrthographicNear;
			data.FarPlane = m_OrthographicFar;
			data.FieldOfView = 0.0f;  // Orthographic has no FOV
		}
		
		data.AspectRatio = m_AspectRatio;
		data.ComputeDerivedMatrices();
		
		return data;
	}
	
} // namespace Lunex
