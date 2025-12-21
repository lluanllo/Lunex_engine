#pragma once

/**
 * @file OrthographicCamera.h
 * @brief 2D orthographic camera
 * 
 * AAA Architecture: OrthographicCamera lives in Scene/Camera/
 * Used for 2D rendering.
 */

#include "CameraData.h"
#include <glm/glm.hpp>

namespace Lunex {

	/**
	 * @class OrthographicCamera
	 * @brief Simple 2D orthographic camera
	 */
	class OrthographicCamera {
	public:
		OrthographicCamera(float left, float right, float bottom, float top);
		
		void SetProjection(float left, float right, float bottom, float top);
		
		// ========== POSITION ==========
		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }
		
		// ========== ROTATION ==========
		float GetRotation() const { return m_Rotation; }
		void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }
		
		// ========== MATRICES ==========
		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
		
		// ========== VIEW DATA (AAA Architecture) ==========
		/**
		 * @brief Generate ViewData for the rendering system
		 */
		ViewData GetViewData() const;
		
	private:
		void RecalculateViewMatrix();
		
	private:
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;
		
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Rotation = 0.0f;
		
		// Store bounds for ViewData
		float m_Left = -1.0f, m_Right = 1.0f;
		float m_Bottom = -1.0f, m_Top = 1.0f;
	};
	
} // namespace Lunex
