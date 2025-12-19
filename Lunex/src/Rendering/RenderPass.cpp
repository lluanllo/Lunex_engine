#include "stpch.h"
#include "RenderPass.h"
#include "Renderer/EditorCamera.h"

namespace Lunex {

	ViewInfo ViewInfo::FromEditorCamera(const EditorCamera& camera, uint32_t width, uint32_t height) {
		ViewInfo info;
		info.ViewMatrix = camera.GetViewMatrix();
		info.ProjectionMatrix = camera.GetProjection();
		info.ViewProjectionMatrix = camera.GetViewProjection();
		info.CameraPosition = camera.GetPosition();
		info.ViewportWidth = width;
		info.ViewportHeight = height;
		info.NearPlane = camera.GetNearClip();
		info.FarPlane = camera.GetFarClip();
		return info;
	}
	
	ViewInfo ViewInfo::FromCamera(const Camera& camera, const glm::mat4& transform, uint32_t width, uint32_t height) {
		ViewInfo info;
		info.ProjectionMatrix = camera.GetProjection();
		info.ViewMatrix = glm::inverse(transform);
		info.ViewProjectionMatrix = info.ProjectionMatrix * info.ViewMatrix;
		info.CameraPosition = glm::vec3(transform[3]);
		info.ViewportWidth = width;
		info.ViewportHeight = height;
		// Note: Camera class doesn't expose near/far, using defaults
		return info;
	}

} // namespace Lunex
