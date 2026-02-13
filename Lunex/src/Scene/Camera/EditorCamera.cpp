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
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), 
		  Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
	{		
		UpdateView();
	}
	
	void EditorCamera::UpdateProjection() {
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}
	
	void EditorCamera::UpdateView() {
		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}
	
	std::pair<float, float> EditorCamera::PanSpeed() const {
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;
		
		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;
		
		return { xFactor, yFactor };
	}
	
	float EditorCamera::RotationSpeed() const {
		return 0.3f;
	}
	
	float EditorCamera::ZoomSpeed() const {
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f);
		return speed;
	}
	
	void EditorCamera::OnUpdate(Timestep ts) {
		auto& inputMgr = InputManager::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		
		bool wasFlyCameraActive = m_IsFlyCameraActive;
		m_IsFlyCameraActive = m_AllowFlyCamera && Input::IsMouseButtonPressed(Mouse::ButtonRight);
		
		if (m_IsFlyCameraActive && !wasFlyCameraActive) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			m_InitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
		}
		else if (!m_IsFlyCameraActive && wasFlyCameraActive) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		
		if (m_IsFlyCameraActive) {
			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			
			MouseRotate(delta);
			
			float moveSpeed = 5.0f * ts;
			
			if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift)) {
				moveSpeed *= 3.0f;
			}
			
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
		else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			
			MousePan(delta);
		}
		else {
			m_InitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
		}
		
		UpdateView();
	}
	
	void EditorCamera::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(LNX_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}
	
	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e) {
		float delta = e.GetYOffset();
		float moveSpeed = 2.0f;
		
		m_Position += GetForwardDirection() * delta * moveSpeed;
		
		UpdateView();
		return false;
	}
	
	void EditorCamera::MousePan(const glm::vec2& delta) {
		auto [xSpeed, ySpeed] = PanSpeed();
		m_Position += -GetRightDirection() * delta.x * xSpeed * 10.0f;
		m_Position += GetUpDirection() * delta.y * ySpeed * 10.0f;
	}
	
	void EditorCamera::MouseRotate(const glm::vec2& delta) {
		m_Yaw += delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
	}
	
	void EditorCamera::MouseZoom(float delta) {
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
		return m_Position;
	}
	
	glm::quat EditorCamera::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
	
	ViewData EditorCamera::GetViewData() const {
		ViewData data;
		data.ViewMatrix = m_ViewMatrix;
		data.ProjectionMatrix = m_Projection;
		data.CameraPosition = m_Position;
		data.CameraDirection = GetForwardDirection();
		data.CameraUp = GetUpDirection();
		data.CameraRight = GetRightDirection();
		data.NearPlane = m_NearClip;
		data.FarPlane = m_FarClip;
		data.FieldOfView = m_FOV;
		data.AspectRatio = m_AspectRatio;
		data.ViewportWidth = static_cast<uint32_t>(m_ViewportWidth);
		data.ViewportHeight = static_cast<uint32_t>(m_ViewportHeight);
		data.ComputeDerivedMatrices();
		return data;
	}
	
} // namespace Lunex
