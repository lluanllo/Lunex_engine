#include "stpch.h"
#include "RenderPass.h"
#include "Scene/Camera/EditorCamera.h"

namespace Lunex {

	ViewInfo ViewInfo::FromEditorCamera(const EditorCamera& camera, uint32_t width, uint32_t height) {
		ViewInfo info;
		info.ViewMatrix = camera.GetViewMatrix();
		info.ProjectionMatrix = camera.GetProjection();
		info.ViewProjectionMatrix = camera.GetViewProjection();
		info.CameraPosition = camera.GetPosition();
		info.CameraDirection = camera.GetForwardDirection();  // NEW
		info.ViewportWidth = width;
		info.ViewportHeight = height;
		info.AspectRatio = width > 0 ? (float)width / (float)height : 1.0f;  // NEW
		info.NearPlane = camera.GetNearClip();
		info.FarPlane = camera.GetFarClip();
		info.IsEditorCamera = true;
		return info;
	}
	
	ViewInfo ViewInfo::FromCamera(const Camera& camera, const glm::mat4& transform, uint32_t width, uint32_t height) {
		ViewInfo info;
		info.ProjectionMatrix = camera.GetProjection();
		info.ViewMatrix = glm::inverse(transform);
		info.ViewProjectionMatrix = info.ProjectionMatrix * info.ViewMatrix;
		info.CameraPosition = glm::vec3(transform[3]);
		info.CameraDirection = -glm::normalize(glm::vec3(transform[2]));  // NEW: -Z axis of transform
		info.ViewportWidth = width;
		info.ViewportHeight = height;
		info.AspectRatio = width > 0 ? (float)width / (float)height : 1.0f;  // NEW
		info.IsEditorCamera = false;
		return info;
	}

} // namespace Lunex
