#include "stpch.h"
#include "CameraData.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	// ============================================================================
	// VIEW FRUSTUM IMPLEMENTATION
	// ============================================================================

	ViewFrustum ViewFrustum::FromViewProjection(const glm::mat4& vp) {
		ViewFrustum frustum;
		
		// Extract planes from view-projection matrix
		// Left plane
		frustum.Planes[Left].Normal.x = vp[0][3] + vp[0][0];
		frustum.Planes[Left].Normal.y = vp[1][3] + vp[1][0];
		frustum.Planes[Left].Normal.z = vp[2][3] + vp[2][0];
		frustum.Planes[Left].Distance = vp[3][3] + vp[3][0];
		
		// Right plane
		frustum.Planes[Right].Normal.x = vp[0][3] - vp[0][0];
		frustum.Planes[Right].Normal.y = vp[1][3] - vp[1][0];
		frustum.Planes[Right].Normal.z = vp[2][3] - vp[2][0];
		frustum.Planes[Right].Distance = vp[3][3] - vp[3][0];
		
		// Bottom plane
		frustum.Planes[Bottom].Normal.x = vp[0][3] + vp[0][1];
		frustum.Planes[Bottom].Normal.y = vp[1][3] + vp[1][1];
		frustum.Planes[Bottom].Normal.z = vp[2][3] + vp[2][1];
		frustum.Planes[Bottom].Distance = vp[3][3] + vp[3][1];
		
		// Top plane
		frustum.Planes[Top].Normal.x = vp[0][3] - vp[0][1];
		frustum.Planes[Top].Normal.y = vp[1][3] - vp[1][1];
		frustum.Planes[Top].Normal.z = vp[2][3] - vp[2][1];
		frustum.Planes[Top].Distance = vp[3][3] - vp[3][1];
		
		// Near plane
		frustum.Planes[Near].Normal.x = vp[0][3] + vp[0][2];
		frustum.Planes[Near].Normal.y = vp[1][3] + vp[1][2];
		frustum.Planes[Near].Normal.z = vp[2][3] + vp[2][2];
		frustum.Planes[Near].Distance = vp[3][3] + vp[3][2];
		
		// Far plane
		frustum.Planes[Far].Normal.x = vp[0][3] - vp[0][2];
		frustum.Planes[Far].Normal.y = vp[1][3] - vp[1][2];
		frustum.Planes[Far].Normal.z = vp[2][3] - vp[2][2];
		frustum.Planes[Far].Distance = vp[3][3] - vp[3][2];
		
		// Normalize all planes
		for (int i = 0; i < 6; i++) {
			float length = glm::length(frustum.Planes[i].Normal);
			frustum.Planes[i].Normal /= length;
			frustum.Planes[i].Distance /= length;
		}
		
		return frustum;
	}

} // namespace Lunex
