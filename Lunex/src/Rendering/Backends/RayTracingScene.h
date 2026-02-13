#pragma once

/**
 * @file RayTracingScene.h
 * @brief CPU-side BVH acceleration structure and GPU scene data for compute-shader ray tracing
 *
 * Builds a linear BVH over all scene triangles and packs it into SSBOs
 * that the path-tracing compute shader can traverse.
 *
 * GPU data layout (all std430):
 *   binding 0 — BVHNode[]   (32 bytes each)
 *   binding 1 — Triangle[]  (80 bytes each)
 *   binding 2 — Material[]  (64 bytes each)
 *   binding 3 — SceneInfo   (camera + light metadata)
 */

#include "Core/Core.h"
#include "RHI/RHI.h"
#include "Scene/Camera/CameraData.h"
#include "Scene/Lighting/LightTypes.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Lunex {

	// Forward
	class Scene;
	struct SceneRenderData;

	// ============================================================================
	// GPU-SIDE STRUCTURES (match GLSL std430)
	// ============================================================================

	struct alignas(16) GPUBVHNode {
		glm::vec3 AABBMin;
		int32_t   LeftOrFirst;   // <0  ? inner (children at abs), >=0 ? leaf (first triangle)
		glm::vec3 AABBMax;
		int32_t   TriCount;      // 0 ? inner, >0 ? leaf
	};
	static_assert(sizeof(GPUBVHNode) == 32);

	struct alignas(16) GPUTriangle {
		glm::vec4 V0;           // xyz = position, w = u0 of texcoord
		glm::vec4 V1;           // xyz = position, w = v0
		glm::vec4 V2;           // xyz = position, w = u1
		glm::vec4 N0;           // xyz = normal,   w = v1
		glm::vec4 N1N2MatID;    // xy = n1.xy, z = packed(n1.z, n2.x), w = materialIndex
	};
	static_assert(sizeof(GPUTriangle) == 80);

	struct alignas(16) GPUMaterial {
		glm::vec4 Albedo;         // xyz = color, w = alpha
		glm::vec4 EmissionAndMetallic; // xyz = emission, w = metallic
		glm::vec4 RoughnessSpecularPad; // x = roughness, y = specular, zw = pad
		glm::vec4 Padding;
	};
	static_assert(sizeof(GPUMaterial) == 64);

	struct alignas(16) GPURTLight {
		glm::vec4 PositionAndType;  // xyz = position, w = type (0=dir, 1=point, 2=spot)
		glm::vec4 DirectionAndRange; // xyz = direction, w = range
		glm::vec4 ColorAndIntensity; // xyz = color, w = intensity
		glm::vec4 Params;           // x = innerCone, y = outerCone, z = castShadow, w = pad
	};
	static_assert(sizeof(GPURTLight) == 64);

	struct alignas(16) GPURTSceneInfo {
		glm::mat4 InverseView;
		glm::mat4 InverseProjection;
		glm::vec4 CameraPositionAndFOV;  // xyz = pos, w = fov
		glm::vec4 ViewportSizeAndFrame;  // x = width, y = height, z = frameIndex, w = spp
		glm::vec4 SkyColorTop;
		glm::vec4 SkyColorBottom;
		int32_t   NumTriangles;
		int32_t   NumBVHNodes;
		int32_t   NumLights;
		int32_t   NumMaterials;
		int32_t   MaxBounces;
		int32_t   AccumulationEnabled;
		float     Padding[2];
	};

	// ============================================================================
	// CPU BVH BUILDER
	// ============================================================================

	struct BVHBuildTriangle {
		glm::vec3 V0, V1, V2;
		glm::vec3 N0, N1, N2;
		glm::vec2 UV0, UV1;
		int MaterialIndex = 0;
		glm::vec3 Centroid() const { return (V0 + V1 + V2) / 3.0f; }
	};

	struct AABB {
		glm::vec3 Min = glm::vec3(1e30f);
		glm::vec3 Max = glm::vec3(-1e30f);

		void Grow(const glm::vec3& p) {
			Min = glm::min(Min, p);
			Max = glm::max(Max, p);
		}
		void Grow(const AABB& other) {
			Min = glm::min(Min, other.Min);
			Max = glm::max(Max, other.Max);
		}
		float SurfaceArea() const {
			glm::vec3 d = Max - Min;
			return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
		}
	};

	// ============================================================================
	// RAY TRACING SCENE
	// ============================================================================

	class RayTracingSceneData {
	public:
		RayTracingSceneData() = default;
		~RayTracingSceneData() = default;

		/**
		 * @brief Build BVH + GPU buffers from scene data.
		 *        Call once per scene change (not per frame).
		 */
		void Build(const SceneRenderData& sceneData, Scene* scene);

		/**
		 * @brief Update only the per-frame uniform (camera, frame index, etc.)
		 */
		void UpdatePerFrame(const CameraRenderData& camera,
			uint32_t viewportW, uint32_t viewportH,
			uint32_t frameIndex, int maxBounces,
			bool accumulate);

		// ---- GPU buffer access ----
		Ref<RHI::RHIStorageBuffer> GetBVHBuffer()      const { return m_BVHBuffer; }
		Ref<RHI::RHIStorageBuffer> GetTriangleBuffer()  const { return m_TriangleBuffer; }
		Ref<RHI::RHIStorageBuffer> GetMaterialBuffer()  const { return m_MaterialBuffer; }
		Ref<RHI::RHIStorageBuffer> GetLightBuffer()     const { return m_LightBuffer; }
		Ref<RHI::RHIUniformBuffer> GetSceneInfoUBO()    const { return m_SceneInfoUBO; }

		bool IsBuilt() const { return m_Built; }
		uint32_t GetTriangleCount() const { return (uint32_t)m_GPUTriangles.size(); }
		uint32_t GetBVHNodeCount()  const { return (uint32_t)m_GPUNodes.size(); }

	private:
		void BuildBVH();
		void UploadToGPU();
		void CollectTriangles(Scene* scene);
		void CollectMaterials(Scene* scene);
		void CollectLights(const SceneRenderData& sceneData);

		// CPU side
		std::vector<BVHBuildTriangle> m_BuildTriangles;
		std::vector<GPUBVHNode>       m_GPUNodes;
		std::vector<GPUTriangle>      m_GPUTriangles;
		std::vector<GPUMaterial>      m_GPUMaterials;
		std::vector<GPURTLight>       m_GPULights;
		GPURTSceneInfo                m_SceneInfo{};

		// GPU side
		Ref<RHI::RHIStorageBuffer>   m_BVHBuffer;
		Ref<RHI::RHIStorageBuffer>   m_TriangleBuffer;
		Ref<RHI::RHIStorageBuffer>   m_MaterialBuffer;
		Ref<RHI::RHIStorageBuffer>   m_LightBuffer;
		Ref<RHI::RHIUniformBuffer>   m_SceneInfoUBO;

		bool m_Built = false;
	};

} // namespace Lunex
