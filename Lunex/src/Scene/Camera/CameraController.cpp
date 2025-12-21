#include "stpch.h"
#include "CameraController.h"
#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
#include "Events/MouseEvent.h"  // NEW: Include MouseScrolledEvent

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Lunex {

	// ============================================================================
	// ORBIT CAMERA CONTROLLER
	// ============================================================================

	OrbitCameraController::OrbitCameraController(float fov, float nearClip, float farClip)
		: m_FOV(fov), m_NearClip(nearClip), m_FarClip(farClip) {
		UpdateViewMatrix();
	}

	void OrbitCameraController::OnUpdate(Timestep ts) {
		if (!m_Enabled) return;
		
		glm::vec2 mousePos = { Input::GetMouseX(), Input::GetMouseY() };
		glm::vec2 delta = (mousePos - m_LastMousePosition) * 0.003f;
		m_LastMousePosition = mousePos;
		
		if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
			if (Input::IsKeyPressed(Key::LeftShift)) {
				Pan(delta);
			} else {
				Rotate(delta);
			}
		}
		
		UpdateViewMatrix();
	}

	void OrbitCameraController::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
			Zoom(e.GetYOffset() * 0.1f);
			UpdateViewMatrix();
			return false;
		});
	}

	ViewData OrbitCameraController::GetViewData() const {
		ViewData data;
		data.ViewMatrix = m_ViewMatrix;
		data.ProjectionMatrix = m_ProjectionMatrix;
		data.CameraPosition = CalculatePosition();
		data.CameraDirection = glm::normalize(m_FocalPoint - data.CameraPosition);
		data.CameraUp = GetUp();
		data.CameraRight = GetRight();
		data.NearPlane = m_NearClip;
		data.FarPlane = m_FarClip;
		data.FieldOfView = m_FOV;
		data.AspectRatio = m_AspectRatio;
		data.ViewportWidth = static_cast<uint32_t>(m_ViewportWidth);
		data.ViewportHeight = static_cast<uint32_t>(m_ViewportHeight);
		data.ComputeDerivedMatrices();
		return data;
	}

	glm::vec3 OrbitCameraController::GetPosition() const {
		return CalculatePosition();
	}

	glm::vec3 OrbitCameraController::GetForward() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 OrbitCameraController::GetUp() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 OrbitCameraController::GetRight() const {
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	void OrbitCameraController::SetPosition(const glm::vec3& position) {
		m_Distance = glm::length(position - m_FocalPoint);
		UpdateViewMatrix();
	}

	void OrbitCameraController::LookAt(const glm::vec3& target) {
		m_FocalPoint = target;
		UpdateViewMatrix();
	}

	void OrbitCameraController::UpdateViewMatrix() {
		m_ProjectionMatrix = glm::perspective(
			glm::radians(m_FOV),
			m_AspectRatio,
			m_NearClip,
			m_FarClip
		);
		
		glm::vec3 position = CalculatePosition();
		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void OrbitCameraController::Pan(const glm::vec2& delta) {
		float x = delta.x * m_PanSpeed * m_Distance * 0.1f;
		float y = delta.y * m_PanSpeed * m_Distance * 0.1f;
		
		m_FocalPoint += -GetRight() * x;
		m_FocalPoint += GetUp() * y;
	}

	void OrbitCameraController::Rotate(const glm::vec2& delta) {
		m_Yaw += delta.x * m_RotationSpeed;
		m_Pitch += delta.y * m_RotationSpeed;
		
		// Clamp pitch
		m_Pitch = glm::clamp(m_Pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
	}

	void OrbitCameraController::Zoom(float delta) {
		m_Distance -= delta * m_ZoomSpeed * m_Distance;
		m_Distance = glm::max(m_Distance, 0.1f);
	}

	glm::vec3 OrbitCameraController::CalculatePosition() const {
		return m_FocalPoint - GetForward() * m_Distance;
	}

	glm::quat OrbitCameraController::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	// ============================================================================
	// FLY CAMERA CONTROLLER
	// ============================================================================

	FlyCameraController::FlyCameraController(float fov, float nearClip, float farClip)
		: m_FOV(fov), m_NearClip(nearClip), m_FarClip(farClip) {
		UpdateVectors();
	}

	void FlyCameraController::OnUpdate(Timestep ts) {
		if (!m_Enabled) return;
		
		glm::vec2 mousePos = { Input::GetMouseX(), Input::GetMouseY() };
		
		// Activate with right mouse button
		bool wasActive = m_Active;
		m_Active = Input::IsMouseButtonPressed(Mouse::ButtonRight);
		
		if (m_Active) {
			if (!wasActive || m_FirstMouse) {
				m_LastMousePosition = mousePos;
				m_FirstMouse = false;
			}
			
			// Mouse look
			glm::vec2 delta = mousePos - m_LastMousePosition;
			m_LastMousePosition = mousePos;
			
			m_Yaw += delta.x * m_MouseSensitivity;
			m_Pitch -= delta.y * m_MouseSensitivity;
			m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
			
			UpdateVectors();
			
			// Movement
			float speed = m_MoveSpeed * ts;
			if (Input::IsKeyPressed(Key::LeftShift)) {
				speed *= m_SprintMultiplier;
			}
			
			if (Input::IsKeyPressed(Key::W)) m_Position += m_Front * speed;
			if (Input::IsKeyPressed(Key::S)) m_Position -= m_Front * speed;
			if (Input::IsKeyPressed(Key::A)) m_Position -= m_Right * speed;
			if (Input::IsKeyPressed(Key::D)) m_Position += m_Right * speed;
			if (Input::IsKeyPressed(Key::E)) m_Position += m_WorldUp * speed;
			if (Input::IsKeyPressed(Key::Q)) m_Position -= m_WorldUp * speed;
		} else {
			m_FirstMouse = true;
		}
	}

	void FlyCameraController::OnEvent(Event& e) {
		// Handle scroll for speed adjustment
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
			if (m_Active) {
				m_MoveSpeed += e.GetYOffset() * 0.5f;
				m_MoveSpeed = glm::max(m_MoveSpeed, 0.1f);
			}
			return false;
		});
	}

	ViewData FlyCameraController::GetViewData() const {
		ViewData data;
		data.ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
		data.ProjectionMatrix = glm::perspective(
			glm::radians(m_FOV),
			m_AspectRatio,
			m_NearClip,
			m_FarClip
		);
		data.CameraPosition = m_Position;
		data.CameraDirection = m_Front;
		data.CameraUp = m_Up;
		data.CameraRight = m_Right;
		data.NearPlane = m_NearClip;
		data.FarPlane = m_FarClip;
		data.FieldOfView = m_FOV;
		data.AspectRatio = m_AspectRatio;
		data.ViewportWidth = static_cast<uint32_t>(m_ViewportWidth);
		data.ViewportHeight = static_cast<uint32_t>(m_ViewportHeight);
		data.ComputeDerivedMatrices();
		return data;
	}

	glm::vec3 FlyCameraController::GetForward() const {
		return m_Front;
	}

	glm::vec3 FlyCameraController::GetUp() const {
		return m_Up;
	}

	glm::vec3 FlyCameraController::GetRight() const {
		return m_Right;
	}

	void FlyCameraController::LookAt(const glm::vec3& target) {
		glm::vec3 direction = glm::normalize(target - m_Position);
		m_Pitch = glm::degrees(asin(direction.y));
		m_Yaw = glm::degrees(atan2(direction.z, direction.x));
		UpdateVectors();
	}

	void FlyCameraController::UpdateVectors() {
		glm::vec3 front;
		front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		front.y = sin(glm::radians(m_Pitch));
		front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		m_Front = glm::normalize(front);
		
		m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
		m_Up = glm::normalize(glm::cross(m_Right, m_Front));
	}

	// ============================================================================
	// FOLLOW CAMERA CONTROLLER
	// ============================================================================

	FollowCameraController::FollowCameraController(float fov, float nearClip, float farClip)
		: m_FOV(fov), m_NearClip(nearClip), m_FarClip(farClip) {
	}

	void FollowCameraController::OnUpdate(Timestep ts) {
		if (!m_Enabled) return;
		
		// Smooth follow with damping
		glm::vec3 desiredPosition = m_TargetPosition + m_Offset;
		
		m_CurrentPosition = glm::mix(
			m_CurrentPosition,
			desiredPosition,
			1.0f - exp(-m_Smoothness * ts.GetSeconds())
		);
	}

	void FollowCameraController::OnEvent(Event& e) {
		// Follow camera typically doesn't respond to input directly
	}

	ViewData FollowCameraController::GetViewData() const {
		ViewData data;
		
		glm::vec3 lookTarget = m_TargetPosition + GetForward() * m_LookAhead;
		data.ViewMatrix = glm::lookAt(m_CurrentPosition, lookTarget, glm::vec3(0.0f, 1.0f, 0.0f));
		data.ProjectionMatrix = glm::perspective(
			glm::radians(m_FOV),
			m_AspectRatio,
			m_NearClip,
			m_FarClip
		);
		data.CameraPosition = m_CurrentPosition;
		data.CameraDirection = GetForward();
		data.CameraUp = GetUp();
		data.CameraRight = GetRight();
		data.NearPlane = m_NearClip;
		data.FarPlane = m_FarClip;
		data.FieldOfView = m_FOV;
		data.AspectRatio = m_AspectRatio;
		data.ViewportWidth = static_cast<uint32_t>(m_ViewportWidth);
		data.ViewportHeight = static_cast<uint32_t>(m_ViewportHeight);
		data.ComputeDerivedMatrices();
		return data;
	}

	glm::vec3 FollowCameraController::GetForward() const {
		return glm::normalize(m_TargetPosition - m_CurrentPosition);
	}

	glm::vec3 FollowCameraController::GetRight() const {
		return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
	}

} // namespace Lunex
