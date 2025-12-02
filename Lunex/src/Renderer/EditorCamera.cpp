#include "stpch.h"
#include "EditorCamera.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
#include "Input/InputManager.h"
#include "Core/Application.h"

#include <glfw/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_QUAT_DATA_WXYZ
#include <glm/gtx/quaternion.hpp>

namespace Lunex {
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
	{		
		UpdateView();
	}
	
	void EditorCamera::UpdateProjection() {
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}
	
	void EditorCamera::UpdateView() {
		// First-person camera: position is directly the camera position, no focal point
		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}
	
	std::pair<float, float> EditorCamera::PanSpeed() const {
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;
		
		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;
		
		return { xFactor, yFactor };
	}
	
	float EditorCamera::RotationSpeed() const {
		return 0.3f; // Reduced for smoother first-person rotation
	}
	
	float EditorCamera::ZoomSpeed() const {
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f); // max speed = 100
		return speed;
	}
	
	void EditorCamera::OnUpdate(Timestep ts) {
		// Use InputManager for better remappable controls
		auto& inputMgr = InputManager::Get();
		
		// Get GLFW window for cursor control
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		
		// ========================================
		// UNREAL ENGINE STYLE FLY CAMERA
		// ========================================
		
		bool wasFlyCameraActive = m_IsFlyCameraActive;
		m_IsFlyCameraActive = Input::IsMouseButtonPressed(Mouse::ButtonRight);
		
		// Activate fly camera mode
		if (m_IsFlyCameraActive && !wasFlyCameraActive) {
			// Just activated - hide cursor and store initial position
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			m_InitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
		}
		// Deactivate fly camera mode
		else if (!m_IsFlyCameraActive && wasFlyCameraActive) {
			// Just deactivated - show cursor
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		
		// ========================================
		// ONLY process input when fly camera is ACTIVE
		// ========================================
		if (m_IsFlyCameraActive) {
			// Fly camera rotation
			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			
			MouseRotate(delta);
			
			// ========================================
			// WASDQE KEYBOARD MOVEMENT
			// ========================================
			float moveSpeed = 5.0f * ts; // 5 units per second
			
			// Boost speed with Shift
			if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift)) {
				moveSpeed *= 3.0f; // 3x faster
			}
			
			// Move camera position directly (first-person style)
			if (Input::IsKeyPressed(Key::W)) {
				m_Position += GetForwardDirection() * moveSpeed;
			}
			if (Input::IsKeyPressed(Key::S)) {
				m_Position -= GetForwardDirection() * moveSpeed;
			}
			if (Input::IsKeyPressed(Key::A)) {
				m_Position -= GetRightDirection() * moveSpeed;
			}
			if (Input::IsKeyPressed(Key::D)) {
				m_Position += GetRightDirection() * moveSpeed;
			}
			if (Input::IsKeyPressed(Key::E)) {
				m_Position += GetUpDirection() * moveSpeed;
			}
			if (Input::IsKeyPressed(Key::Q)) {
				m_Position -= GetUpDirection() * moveSpeed;
			}
		}
		// Middle Mouse Button: Pan camera (only when NOT in fly mode)
		else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			
			MousePan(delta);
		}
		else {
			// Reset mouse position when not dragging
			m_InitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
		}
		
		UpdateView();
	}
	
	void EditorCamera::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(LNX_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}
	
	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e) {
		// Zoom by moving camera forward/backward
		float delta = e.GetYOffset();
		float moveSpeed = 2.0f; // Adjust zoom speed
		
		m_Position += GetForwardDirection() * delta * moveSpeed;
		
		UpdateView();
		return false;
	}
	
	void EditorCamera::MousePan(const glm::vec2& delta) {
		auto [xSpeed, ySpeed] = PanSpeed();
		// Pan the camera position directly
		m_Position += -GetRightDirection() * delta.x * xSpeed * 10.0f;
		m_Position += GetUpDirection() * delta.y * ySpeed * 10.0f;
	}
	
	void EditorCamera::MouseRotate(const glm::vec2& delta) {
		// First-person rotation (no gimbal lock protection for smoother feel)
		m_Yaw += delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
		
		// Clamp pitch to avoid flipping
		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
	}
	
	void EditorCamera::MouseZoom(float delta) {
		// Zoom by moving forward/backward
		m_Position += GetForwardDirection() * delta * 5.0f;
	}
	
	glm::vec3 EditorCamera::GetUpDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	
	glm::vec3 EditorCamera::GetRightDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}
	
	glm::vec3 EditorCamera::GetForwardDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}
	
	glm::vec3 EditorCamera::CalculatePosition() const {
		// First-person camera: position is the camera position itself
		return m_Position;
	}
	
	glm::quat EditorCamera::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
}