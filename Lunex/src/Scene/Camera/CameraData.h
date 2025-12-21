#pragma once

/**
 * @file CameraData.h
 * @brief Data structures for camera view information
 * 
 * AAA Architecture: These structs contain only data that the renderer needs.
 * The renderer NEVER knows about cameras directly - only receives ViewData.
 */

#include <glm/glm.hpp>
#include <cstdint>

namespace Lunex {

	// ============================================================================
	// VIEW DATA - What the renderer receives
	// ============================================================================
	
	/**
	 * @struct ViewData
	 * @brief Minimal view information for rendering
	 */
	struct ViewData {
		glm::mat4 ViewMatrix = glm::mat4(1.0f);
		glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 InverseViewMatrix = glm::mat4(1.0f);
		glm::mat4 InverseProjectionMatrix = glm::mat4(1.0f);
		
		glm::vec3 CameraPosition = glm::vec3(0.0f);
		glm::vec3 CameraDirection = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 CameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 CameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
		
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;
		float FieldOfView = 45.0f;
		float AspectRatio = 16.0f / 9.0f;
		
		uint32_t ViewportWidth = 1920;
		uint32_t ViewportHeight = 1080;
		uint32_t ViewportX = 0;
		uint32_t ViewportY = 0;
		
		glm::mat4 PreviousViewProjectionMatrix = glm::mat4(1.0f);
		glm::vec2 Jitter = glm::vec2(0.0f);
		
		void ComputeDerivedMatrices() {
			ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
			InverseViewMatrix = glm::inverse(ViewMatrix);
			InverseProjectionMatrix = glm::inverse(ProjectionMatrix);
		}
	};

	// ============================================================================
	// CAMERA RENDER DATA - For RenderSystem integration
	// ============================================================================
	
	/**
	 * @struct CameraRenderData
	 * @brief GPU-ready camera data for rendering
	 * 
	 * This is what RenderSystem::GetActiveCameraData() returns.
	 */
	struct CameraRenderData {
		glm::mat4 ViewMatrix = glm::mat4(1.0f);
		glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 InverseViewMatrix = glm::mat4(1.0f);
		glm::mat4 InverseProjectionMatrix = glm::mat4(1.0f);
		
		glm::vec3 Position = glm::vec3(0.0f);
		glm::vec3 Direction = glm::vec3(0.0f, 0.0f, -1.0f);
		
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;
		float FieldOfView = 45.0f;
		float AspectRatio = 16.0f / 9.0f;
		
		bool IsPerspective = true;
		
		// Construct from ViewData
		static CameraRenderData FromViewData(const ViewData& view) {
			CameraRenderData data;
			data.ViewMatrix = view.ViewMatrix;
			data.ProjectionMatrix = view.ProjectionMatrix;
			data.ViewProjectionMatrix = view.ViewProjectionMatrix;
			data.InverseViewMatrix = view.InverseViewMatrix;
			data.InverseProjectionMatrix = view.InverseProjectionMatrix;
			data.Position = view.CameraPosition;
			data.Direction = view.CameraDirection;
			data.NearPlane = view.NearPlane;
			data.FarPlane = view.FarPlane;
			data.FieldOfView = view.FieldOfView;
			data.AspectRatio = view.AspectRatio;
			data.IsPerspective = true;
			return data;
		}
	};

	// ============================================================================
	// FRUSTUM DATA - For culling
	// ============================================================================
	
	/**
	 * @struct FrustumPlane
	 * @brief Single frustum plane for culling
	 */
	struct FrustumPlane {
		glm::vec3 Normal = glm::vec3(0.0f);
		float Distance = 0.0f;
		
		float DistanceToPoint(const glm::vec3& point) const {
			return glm::dot(Normal, point) + Distance;
		}
	};
	
	/**
	 * @struct ViewFrustum
	 * @brief 6 planes defining the view frustum for culling
	 */
	struct ViewFrustum {
		FrustumPlane Planes[6];  // Left, Right, Bottom, Top, Near, Far
		
		enum PlaneIndex {
			Left = 0,
			Right = 1,
			Bottom = 2,
			Top = 3,
			Near = 4,
			Far = 5
		};
		
		ViewFrustum() = default;
		
		// Constructor from view-projection matrix
		explicit ViewFrustum(const glm::mat4& vp) {
			*this = FromViewProjection(vp);
		}
		
		// Check if a point is inside the frustum
		bool ContainsPoint(const glm::vec3& point) const {
			for (int i = 0; i < 6; i++) {
				if (Planes[i].DistanceToPoint(point) < 0.0f) {
					return false;
				}
			}
			return true;
		}
		
		// Check if a sphere intersects the frustum
		bool IntersectsSphere(const glm::vec3& center, float radius) const {
			for (int i = 0; i < 6; i++) {
				if (Planes[i].DistanceToPoint(center) < -radius) {
					return false;
				}
			}
			return true;
		}
		
		// Check if an AABB intersects the frustum
		bool IntersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
			for (int i = 0; i < 6; i++) {
				glm::vec3 positive = min;
				if (Planes[i].Normal.x >= 0) positive.x = max.x;
				if (Planes[i].Normal.y >= 0) positive.y = max.y;
				if (Planes[i].Normal.z >= 0) positive.z = max.z;
				
				if (Planes[i].DistanceToPoint(positive) < 0.0f) {
					return false;
				}
			}
			return true;
		}
		
		// Build frustum from view-projection matrix
		static ViewFrustum FromViewProjection(const glm::mat4& vp);
	};

	// ============================================================================
	// CAMERA RENDER INFO - Extended info for advanced rendering
	// ============================================================================
	
	/**
	 * @struct CameraRenderInfo
	 * @brief Extended camera information for advanced rendering features
	 */
	struct CameraRenderInfo {
		ViewData View;
		ViewFrustum Frustum;
		
		uint32_t ViewIndex = 0;
		bool IsPrimaryView = true;
		bool IsReflectionView = false;
		bool IsShadowView = false;
		
		float LODBias = 0.0f;
		float MinDrawDistance = 0.0f;
		float MaxDrawDistance = 10000.0f;
	};

} // namespace Lunex
