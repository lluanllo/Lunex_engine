#pragma once

/**
 * @file EditorCamera.h
 * @brief Editor camera with fly/orbit controls
 * 
 * AAA Architecture: EditorCamera lives in Scene/Camera/
 * Produces ViewData for the renderer.
 */

#include "Camera.h"
#include "CameraData.h"
#include "Core/Timestep.h"
#include "Events/Event.h"
#include "Events/MouseEvent.h"

#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * @class EditorCamera
	 * @brief First-person/orbit camera for the editor
	 */
	class EditorCamera : public Camera {
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);
		
		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);
		
		// ========== DISTANCE (for orbit mode) ==========
		float GetDistance() const { return m_Distance; }
		void SetDistance(float distance) { m_Distance = distance; }
		
		// ========== CLIP PLANES ==========
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip() const { return m_FarClip; }
		
		// ========== POSITION ==========
		void SetPosition(const glm::vec3& position) { m_Position = position; UpdateView(); }

		// Orient the camera towards a world point (keeps position)
		void LookAt(const glm::vec3& target);
		// Frame a bounding sphere (keeps orientation, moves back far enough to fit it)
		void Focus(const glm::vec3& center, float radius);
		const glm::vec3& GetPosition() const { return m_Position; }
		
		// ========== VIEWPORT ==========
		void SetViewportSize(float width, float height) { 
			m_ViewportWidth = width; 
			m_ViewportHeight = height; 
			UpdateProjection(); 
		}
		
		// ========== MATRICES ==========
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }
		
		// ========== DIRECTION VECTORS ==========
		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		glm::quat GetOrientation() const;
		
		// ========== ROTATION ==========
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		
		// ========== FLY CAMERA STATE ==========
		bool IsFlyCameraActive() const { return m_IsFlyCameraActive; }
		void SetAllowFlyCamera(bool allow) { m_AllowFlyCamera = allow; }

		// ========== VIEWS (Blender numpad / navigation gizmo) ==========
		enum class ViewAxis { None = 0, Front, Back, Right, Left, Top, Bottom };
		// Animated snap to an axis view around the focal point; switches to orthographic (auto perspective)
		void SnapToAxis(ViewAxis axis);
		// Rotate around the focal point (radians); leaves an axis view (back to perspective if auto)
		void Orbit(float yawDelta, float pitchDelta);
		void ViewOpposite();
		void SetOrthographic(bool ortho);
		void ToggleOrthographic() { SetOrthographic(!m_Orthographic); m_AutoOrtho = false; }
		bool IsOrthographic() const { return m_Orthographic; }
		ViewAxis GetAxisView() const { return m_AxisView; }
		const char* GetViewName() const;
		// Point the camera orbits around (in front of the camera at the orbit distance)
		glm::vec3 GetFocalPoint() const;
		// Copy a world transform (e.g. a scene camera): position + look direction
		void SetFromTransform(const glm::vec3& position, const glm::vec3& forward);
		
		// ========== VIEW DATA (AAA Architecture) ==========
		/**
		 * @brief Generate ViewData for the rendering system
		 * This is what the renderer receives - no direct camera access.
		 */
		ViewData GetViewData() const;
		
	private:
		void UpdateProjection();
		void UpdateView();
		
		bool OnMouseScroll(MouseScrolledEvent& e);
		
		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);
		
		glm::vec3 CalculatePosition() const;
		
		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
		
	private:
		float m_FOV = 45.0f;
		float m_AspectRatio = 1.778f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		
		glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };
		
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f;
		float m_Yaw = 0.0f;
		
		float m_ViewportWidth = 1280;
		float m_ViewportHeight = 720;
		
		bool m_IsFlyCameraActive = false;
		bool m_AllowFlyCamera = false;
		bool m_IsPanning = false;

		// Views
		bool m_Orthographic = false;
		bool m_AutoOrtho = false;          // entered ortho by snapping to an axis: rotating goes back to perspective
		float m_OrthoSize = 5.0f;          // half height of the view in world units
		float m_OrthoBack = 0.0f;          // ortho eye pulled back by this much (nothing gets clipped)
		ViewAxis m_AxisView = ViewAxis::None;
		bool m_Animating = false;
		float m_AnimT = 0.0f;
		float m_FromPitch = 0.0f, m_FromYaw = 0.0f, m_ToPitch = 0.0f, m_ToYaw = 0.0f;
		glm::vec3 m_AnimFocal{ 0.0f };
	};
	
} // namespace Lunex
