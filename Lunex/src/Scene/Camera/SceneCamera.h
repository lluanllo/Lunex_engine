#pragma once

/**
 * @file SceneCamera.h
 * @brief Runtime scene camera (perspective/orthographic)
 * 
 * AAA Architecture: SceneCamera lives in Scene/Camera/
 * Used by CameraComponent for in-game cameras.
 */

#include "Camera.h"
#include "CameraData.h"
#include "Core/Core.h"

#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * @class SceneCamera
	 * @brief Camera for in-game use with perspective/orthographic projection
	 */
	class SceneCamera : public Camera {
	public:
		enum class ProjectionType { Perspective = 0, Orthographic = 1 };
		
	public:
		SceneCamera();
		virtual ~SceneCamera() = default;
		
		// ========== PROJECTION SETUP ==========
		void SetOrthographic(float size, float nearClip, float farClip);		
		void SetPerspective(float verticalFOV, float nearClip, float farClip);
		
		void SetViewportSize(uint32_t width, uint32_t height);
		
		// ========== PERSPECTIVE PROPERTIES ==========
		float GetPerspectiveVerticalFOV() const { return m_PerspectiveFOV; }
		void SetPerspectiveVerticalFOV(float verticalFov) { m_PerspectiveFOV = verticalFov; RecalculateProjection(); }
		float GetPerspectiveNearClip() const { return m_PerspectiveNear; }
		void SetPerspectiveNearClip(float nearClip) { m_PerspectiveNear = nearClip; RecalculateProjection(); }
		float GetPerspectiveFarClip() const { return m_PerspectiveFar; }
		void SetPerspectiveFarClip(float farClip) { m_PerspectiveFar = farClip; RecalculateProjection(); }
		
		// ========== ORTHOGRAPHIC PROPERTIES ==========
		float GetOrthographicSize() const { return m_OrthographicSize; }
		void SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjection(); }
		float GetOrthographicNearClip() const { return m_OrthographicNear; }
		void SetOrthographicNearClip(float nearClip) { m_OrthographicNear = nearClip; RecalculateProjection(); }
		float GetOrthographicFarClip() const { return m_OrthographicFar; }
		void SetOrthographicFarClip(float farClip) { m_OrthographicFar = farClip; RecalculateProjection(); }
		
		// ========== PROJECTION TYPE ==========
		ProjectionType GetProjectionType() const { return m_ProjectionType; }
		void SetProjectionType(ProjectionType type) { m_ProjectionType = type; RecalculateProjection(); }
		
		float GetAspectRatio() const { return m_AspectRatio; }
		
		// ========== VIEW DATA (AAA Architecture) ==========
		/**
		 * @brief Generate ViewData for the rendering system
		 * Requires transform to be passed in since SceneCamera doesn't store position.
		 */
		ViewData GetViewData(const glm::mat4& transform) const;
		
	private:
		void RecalculateProjection();
		
	private:
		ProjectionType m_ProjectionType = ProjectionType::Orthographic;
		
		float m_PerspectiveFOV = glm::radians(45.0f);
		float m_PerspectiveNear = 0.01f;
		float m_PerspectiveFar = 1000.0f;
		
		float m_OrthographicSize = 10.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;
		
		float m_AspectRatio = 0.0f;
	};
	
} // namespace Lunex
