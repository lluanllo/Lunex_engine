#pragma once

/**
 * @file CameraController.h
 * @brief Base class for camera controllers
 * 
 * AAA Architecture:
 * - Controllers handle INPUT and update camera state
 * - Decoupled from camera data itself
 * - Multiple controller types: Orbit, Fly, Follow, Rail, etc.
 */

#include "CameraData.h"
#include "Core/Core.h"
#include "Core/Timestep.h"
#include "Events/Event.h"

#include <glm/glm.hpp>

namespace Lunex {

	// ============================================================================
	// CAMERA CONTROLLER BASE
	// ============================================================================
	
	/**
	 * @class CameraController
	 * @brief Abstract base for all camera controllers
	 */
	class CameraController {
	public:
		virtual ~CameraController() = default;
		
		// ========== LIFECYCLE ==========
		virtual void OnUpdate(Timestep ts) = 0;
		virtual void OnEvent(Event& e) = 0;
		
		// ========== VIEW GENERATION ==========
		
		/**
		 * @brief Generate ViewData from current controller state
		 */
		virtual ViewData GetViewData() const = 0;
		
		// ========== COMMON PROPERTIES ==========
		
		virtual void SetViewportSize(float width, float height) {
			m_ViewportWidth = width;
			m_ViewportHeight = height;
			m_AspectRatio = width / height;
		}
		
		float GetAspectRatio() const { return m_AspectRatio; }
		float GetViewportWidth() const { return m_ViewportWidth; }
		float GetViewportHeight() const { return m_ViewportHeight; }
		
		// ========== CAMERA STATE ==========
		
		virtual glm::vec3 GetPosition() const = 0;
		virtual glm::vec3 GetForward() const = 0;
		virtual glm::vec3 GetUp() const = 0;
		virtual glm::vec3 GetRight() const = 0;
		
		virtual void SetPosition(const glm::vec3& position) = 0;
		virtual void LookAt(const glm::vec3& target) = 0;
		
		// ========== CONTROLLER ENABLE ==========
		
		void SetEnabled(bool enabled) { m_Enabled = enabled; }
		bool IsEnabled() const { return m_Enabled; }
		
	protected:
		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;
		float m_AspectRatio = 16.0f / 9.0f;
		
		bool m_Enabled = true;
	};

	// ============================================================================
	// ORBIT CAMERA CONTROLLER
	// ============================================================================
	
	/**
	 * @class OrbitCameraController
	 * @brief Orbits around a focal point (common for editors)
	 */
	class OrbitCameraController : public CameraController {
	public:
		OrbitCameraController(float fov = 45.0f, float nearClip = 0.1f, float farClip = 1000.0f);
		
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		ViewData GetViewData() const override;
		
		// Position/orientation
		glm::vec3 GetPosition() const override;
		glm::vec3 GetForward() const override;
		glm::vec3 GetUp() const override;
		glm::vec3 GetRight() const override;
		
		void SetPosition(const glm::vec3& position) override;
		void LookAt(const glm::vec3& target) override;
		
		// Orbit-specific
		void SetFocalPoint(const glm::vec3& point) { m_FocalPoint = point; }
		const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }
		
		void SetDistance(float distance) { m_Distance = distance; }
		float GetDistance() const { return m_Distance; }
		
		void SetPitch(float pitch) { m_Pitch = pitch; }
		void SetYaw(float yaw) { m_Yaw = yaw; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		
		// FOV and clipping
		void SetFOV(float fov) { m_FOV = fov; }
		float GetFOV() const { return m_FOV; }
		
		void SetNearClip(float nearClip) { m_NearClip = nearClip; }
		void SetFarClip(float farClip) { m_FarClip = farClip; }
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip() const { return m_FarClip; }
		
		// Sensitivity
		void SetPanSpeed(float speed) { m_PanSpeed = speed; }
		void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }
		void SetZoomSpeed(float speed) { m_ZoomSpeed = speed; }
		
	private:
		void UpdateViewMatrix();
		void Pan(const glm::vec2& delta);
		void Rotate(const glm::vec2& delta);
		void Zoom(float delta);
		
		glm::vec3 CalculatePosition() const;
		glm::quat GetOrientation() const;
		
	private:
		glm::vec3 m_FocalPoint = glm::vec3(0.0f);
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f;
		float m_Yaw = 0.0f;
		
		float m_FOV = 45.0f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		
		float m_PanSpeed = 1.0f;
		float m_RotationSpeed = 0.8f;
		float m_ZoomSpeed = 1.0f;
		
		glm::vec2 m_LastMousePosition = glm::vec2(0.0f);
		
		// Cached matrices
		glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
	};

	// ============================================================================
	// FLY CAMERA CONTROLLER
	// ============================================================================
	
	/**
	 * @class FlyCameraController
	 * @brief First-person fly camera (WASD + mouse look)
	 */
	class FlyCameraController : public CameraController {
	public:
		FlyCameraController(float fov = 45.0f, float nearClip = 0.1f, float farClip = 1000.0f);
		
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		ViewData GetViewData() const override;
		
		// Position/orientation
		glm::vec3 GetPosition() const override { return m_Position; }
		glm::vec3 GetForward() const override;
		glm::vec3 GetUp() const override;
		glm::vec3 GetRight() const override;
		
		void SetPosition(const glm::vec3& position) override { m_Position = position; }
		void LookAt(const glm::vec3& target) override;
		
		// Fly-specific
		void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
		float GetMoveSpeed() const { return m_MoveSpeed; }
		
		void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
		float GetMouseSensitivity() const { return m_MouseSensitivity; }
		
		void SetSprintMultiplier(float multiplier) { m_SprintMultiplier = multiplier; }
		
		// Activation (right-click to look)
		void SetActive(bool active) { m_Active = active; }
		bool IsActive() const { return m_Active; }
		
	private:
		void UpdateVectors();
		
	private:
		glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 5.0f);
		glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		float m_Yaw = -90.0f;
		float m_Pitch = 0.0f;
		
		float m_FOV = 45.0f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		
		float m_MoveSpeed = 5.0f;
		float m_MouseSensitivity = 0.1f;
		float m_SprintMultiplier = 2.0f;
		
		bool m_Active = false;
		glm::vec2 m_LastMousePosition = glm::vec2(0.0f);
		bool m_FirstMouse = true;
	};

	// ============================================================================
	// FOLLOW CAMERA CONTROLLER
	// ============================================================================
	
	/**
	 * @class FollowCameraController
	 * @brief Third-person follow camera
	 */
	class FollowCameraController : public CameraController {
	public:
		FollowCameraController(float fov = 45.0f, float nearClip = 0.1f, float farClip = 1000.0f);
		
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		ViewData GetViewData() const override;
		
		// Position/orientation
		glm::vec3 GetPosition() const override { return m_CurrentPosition; }
		glm::vec3 GetForward() const override;
		glm::vec3 GetUp() const override { return glm::vec3(0.0f, 1.0f, 0.0f); }
		glm::vec3 GetRight() const override;
		
		void SetPosition(const glm::vec3& position) override { m_CurrentPosition = position; }
		void LookAt(const glm::vec3& target) override { m_TargetPosition = target; }
		
		// Follow-specific
		void SetTarget(const glm::vec3& position) { m_TargetPosition = position; }
		const glm::vec3& GetTarget() const { return m_TargetPosition; }
		
		void SetOffset(const glm::vec3& offset) { m_Offset = offset; }
		const glm::vec3& GetOffset() const { return m_Offset; }
		
		void SetSmoothness(float smoothness) { m_Smoothness = smoothness; }
		float GetSmoothness() const { return m_Smoothness; }
		
		void SetLookAhead(float lookAhead) { m_LookAhead = lookAhead; }
		
	private:
		glm::vec3 m_TargetPosition = glm::vec3(0.0f);
		glm::vec3 m_CurrentPosition = glm::vec3(0.0f, 5.0f, 10.0f);
		glm::vec3 m_Offset = glm::vec3(0.0f, 5.0f, 10.0f);
		glm::vec3 m_Velocity = glm::vec3(0.0f);
		
		float m_FOV = 45.0f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		
		float m_Smoothness = 5.0f;
		float m_LookAhead = 0.0f;
	};

} // namespace Lunex
