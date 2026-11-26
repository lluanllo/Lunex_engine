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
		m_AspectRatio = m_ViewportWidth / std::max(m_ViewportHeight, 1.0f);
		if (m_Orthographic) {
			const float h = m_OrthoSize, w = h * m_AspectRatio;
			m_Projection = glm::ortho(-w, w, -h, h, m_NearClip, m_FarClip);
		}
		else {
			m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
		}
	}

	// ============================================================================
	// VIEWS (numpad / navigation gizmo)
	// ============================================================================

	namespace {
		constexpr float kHalfPi = 1.57079632679f;
		constexpr float kPi = 3.14159265359f;
		float WrapAngle(float a) { while (a > kPi) a -= 2.0f * kPi; while (a < -kPi) a += 2.0f * kPi; return a; }
	}

	glm::vec3 EditorCamera::GetFocalPoint() const {
		return m_Position + GetForwardDirection() * (m_Distance + m_OrthoBack);
	}

	void EditorCamera::SetOrthographic(bool ortho) {
		if (ortho == m_Orthographic) return;
		const glm::vec3 focal = GetFocalPoint();
		const float tanHalf = std::tan(glm::radians(m_FOV) * 0.5f);
		if (ortho) {
			m_OrthoSize = std::max(0.01f, m_Distance * tanHalf);
			m_OrthoBack = std::max(200.0f, m_Distance * 4.0f);   // eye pulled back: nothing near the focal point gets clipped
		}
		else {
			m_Distance = std::max(0.05f, m_OrthoSize / tanHalf);
			m_OrthoBack = 0.0f;
		}
		m_Orthographic = ortho;
		m_Position = focal - GetForwardDirection() * (m_Distance + m_OrthoBack);
		UpdateProjection();
		UpdateView();
	}

	void EditorCamera::SnapToAxis(ViewAxis axis) {
		float pitch = 0.0f, yaw = 0.0f;
		switch (axis) {
			case ViewAxis::Front:  pitch = 0.0f; yaw = 0.0f; break;         // from +Z looking -Z
			case ViewAxis::Back:   pitch = 0.0f; yaw = kPi; break;
			case ViewAxis::Right:  pitch = 0.0f; yaw = -kHalfPi; break;     // from +X
			case ViewAxis::Left:   pitch = 0.0f; yaw = kHalfPi; break;
			case ViewAxis::Top:    pitch = kHalfPi; yaw = 0.0f; break;      // from +Y, -Z up on screen
			case ViewAxis::Bottom: pitch = -kHalfPi; yaw = 0.0f; break;
			default: return;
		}
		if (!m_Orthographic) { SetOrthographic(true); m_AutoOrtho = true; }
		m_AnimFocal = GetFocalPoint();
		m_FromPitch = m_Pitch;
		m_FromYaw = m_Yaw;
		m_ToPitch = pitch;
		m_ToYaw = m_Yaw + WrapAngle(yaw - m_Yaw);
		m_AnimT = 0.0f;
		m_Animating = true;
		m_AxisView = axis;
	}

	void EditorCamera::Orbit(float yawDelta, float pitchDelta) {
		const glm::vec3 focal = GetFocalPoint();
		m_Animating = false;
		if (m_AxisView != ViewAxis::None && m_AutoOrtho && m_Orthographic) {
			SetOrthographic(false);
			m_AutoOrtho = false;
		}
		m_AxisView = ViewAxis::None;
		m_Yaw += yawDelta;
		m_Pitch = glm::clamp(m_Pitch + pitchDelta, -kHalfPi, kHalfPi);
		m_Position = focal - GetForwardDirection() * (m_Distance + m_OrthoBack);
		UpdateView();
	}

	void EditorCamera::ViewOpposite() {
		switch (m_AxisView) {
			case ViewAxis::Front: SnapToAxis(ViewAxis::Back); return;
			case ViewAxis::Back: SnapToAxis(ViewAxis::Front); return;
			case ViewAxis::Right: SnapToAxis(ViewAxis::Left); return;
			case ViewAxis::Left: SnapToAxis(ViewAxis::Right); return;
			case ViewAxis::Top: SnapToAxis(ViewAxis::Bottom); return;
			case ViewAxis::Bottom: SnapToAxis(ViewAxis::Top); return;
			default: Orbit(kPi, 0.0f); return;
		}
	}

	const char* EditorCamera::GetViewName() const {
		static const char* names[] = { "User", "Front", "Back", "Right", "Left", "Top", "Bottom" };
		static char buf[48];
		snprintf(buf, sizeof(buf), "%s %s", names[static_cast<int>(m_AxisView)], m_Orthographic ? "Orthographic" : "Perspective");
		return buf;
	}

	void EditorCamera::SetFromTransform(const glm::vec3& position, const glm::vec3& forward) {
		if (m_Orthographic) { SetOrthographic(false); m_AutoOrtho = false; }
		m_Animating = false;
		m_AxisView = ViewAxis::None;
		const glm::vec3 d = glm::normalize(forward);
		m_Position = position;
		m_Pitch = std::asin(glm::clamp(-d.y, -1.0f, 1.0f));
		m_Yaw = std::atan2(d.x, -d.z);
		UpdateView();
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

		// Smooth view transition (numpad / gizmo snaps)
		if (m_Animating) {
			m_AnimT = std::min(1.0f, m_AnimT + static_cast<float>(ts) / 0.18f);
			const float k = m_AnimT * m_AnimT * (3.0f - 2.0f * m_AnimT);
			m_Pitch = glm::mix(m_FromPitch, m_ToPitch, k);
			m_Yaw = glm::mix(m_FromYaw, m_ToYaw, k);
			m_Position = m_AnimFocal - GetForwardDirection() * (m_Distance + m_OrthoBack);
			if (m_AnimT >= 1.0f) m_Animating = false;
		}
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
			
			// Rotating leaves an axis view (and its automatic orthographic projection)
			if (delta != glm::vec2(0.0f) && m_AxisView != ViewAxis::None) {
				m_AxisView = ViewAxis::None;
				m_Animating = false;
				if (m_AutoOrtho && m_Orthographic) SetOrthographic(false);
				m_AutoOrtho = false;
			}
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
		else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle) && (m_IsPanning || m_AllowFlyCamera)) {
			// The pan must start over the viewport (middle-drag in other panels, e.g. the node
			// editor, must not move the camera); once started it continues outside it
			const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			if (!m_IsPanning) { m_IsPanning = true; m_InitialMousePosition = mouse; }
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			
			MousePan(delta);
		}
		else {
			m_IsPanning = false;
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
		if (m_Orthographic) {
			// Orthographic zoom = view size
			m_OrthoSize = glm::clamp(m_OrthoSize * std::pow(0.88f, delta), 0.01f, 5000.0f);
			UpdateProjection();
			return false;
		}
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
		m_Pitch = glm::clamp(m_Pitch, -1.5707963f, 1.5707963f);   // radians
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
	
	void EditorCamera::LookAt(const glm::vec3& target) {
		glm::vec3 d = target - m_Position;
		float len = glm::length(d);
		if (len < 1e-5f) return;
		d /= len;
		// Inverse of forward = (cos p sin y, -sin p, -cos p cos y) for orientation quat(-pitch, -yaw, 0)
		m_Pitch = std::asin(glm::clamp(-d.y, -1.0f, 1.0f));
		m_Yaw = std::atan2(d.x, -d.z);
		m_FocalPoint = target;
		m_Distance = len;
		UpdateView();
	}

	void EditorCamera::Focus(const glm::vec3& center, float radius) {
		radius = std::max(radius, 0.05f);
		float distance = radius / std::sin(glm::radians(m_FOV) * 0.5f) * 1.2f;
		m_Position = center - GetForwardDirection() * distance;
		m_FocalPoint = center;
		m_Distance = distance;
		UpdateView();
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
