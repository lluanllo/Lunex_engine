#pragma once

/**
 * @file MeshGeometryPool.h
 * @brief Flattens scene geometry into GPU buffers for ray tracing
 *
 * Walks every SceneDrawItem, transforms vertices to world space,
 * builds a BVH (SAH) and uploads triangles + BVH nodes as SSBOs.
 *
 * Phase 2: AABB computation is parallelized via JobSystem::ParallelFor.
 */

#include "Core/Core.h"
#include "Renderer/StorageBuffer.h"
#include "SceneRenderData.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Lunex {

	// ====================================================================
	// GPU-READY TRIANGLE (80 bytes, std430-friendly)
	// ====================================================================

	struct RTTriangleGPU {
		glm::vec4 V0;                // xyz=pos, w=nx
		glm::vec4 V1;                // xyz=pos, w=ny
		glm::vec4 V2;                // xyz=pos, w=nz
		glm::vec4 TexCoords01;       // xy=uv0, zw=uv1
		glm::vec4 TexCoords2AndMat;  // xy=uv2, z=materialIndex, w=entityID
	};

	// ====================================================================
	// GPU-READY BVH NODE (32 bytes)
	// ====================================================================

	struct RTBVHNodeGPU {
		glm::vec4 BoundsMin;  // xyz=aabb min, w = leftChild OR triStart
		glm::vec4 BoundsMax;  // xyz=aabb max, w = triCount (0 = internal)
	};

	// ====================================================================
	// AABB HELPER
	// ====================================================================

	struct RTAABB {
		glm::vec3 Min = glm::vec3( 1e30f);
		glm::vec3 Max = glm::vec3(-1e30f);

		void Expand(const glm::vec3& p) {
			Min = glm::min(Min, p);
			Max = glm::max(Max, p);
		}
		void Expand(const RTAABB& other) {
			Min = glm::min(Min, other.Min);
			Max = glm::max(Max, other.Max);
		}
		glm::vec3 Center() const { return (Min + Max) * 0.5f; }
		glm::vec3 Extent() const { return Max - Min; }
		float SurfaceArea() const {
			glm::vec3 e = Extent();
			return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
		}
		bool IsValid() const { return Min.x <= Max.x; }
	};

	// ====================================================================
	// BUILD RESULT
	// ====================================================================

	struct MeshPoolBuildResult {
		uint32_t TriangleCount = 0;
		uint32_t BVHNodeCount  = 0;
		float    BuildTimeMs   = 0.0f;
	};

	// ====================================================================
	// MESH GEOMETRY POOL
	// ====================================================================

	class MeshGeometryPool {
	public:
		MeshGeometryPool() = default;
		~MeshGeometryPool() = default;

		void Initialize();
		void Shutdown();

		/**
		 * @brief Flatten scene draw items into triangles, build BVH, upload.
		 * @param items       Draw items collected by SceneDataCollector
		 * @param getMaterialIndex  Callback to resolve material -> GPU index
		 * @return build statistics
		 */
		MeshPoolBuildResult Build(
			const std::vector<SceneDrawItem>& items,
			std::function<uint32_t(const Ref<MaterialInstance>&)> getMaterialIndex);

		/** Bind triangle + BVH SSBOs for the compute shader. */
		void BindForRayTracing(uint32_t triangleBinding, uint32_t bvhBinding) const;

		uint32_t GetTriangleCount() const { return m_TriangleCount; }
		uint32_t GetBVHNodeCount()  const { return m_BVHNodeCount; }

	private:
		// -- Flatten geometry (step 1) -----------------------------------
		void FlattenMeshes(const std::vector<SceneDrawItem>& items,
		                   std::function<uint32_t(const Ref<MaterialInstance>&)> getMaterialIndex);

		// -- Compute AABBs in parallel (step 2) --------------------------
		void ComputeAABBsParallel();

		// -- BVH build (SAH) (step 3) ------------------------------------
		void BuildBVH();
		void BuildBVHRecursive(uint32_t nodeIdx, uint32_t start, uint32_t end, int depth);

		// -- Upload to GPU (step 4) --------------------------------------
		void UploadToGPU();

	private:
		std::vector<RTTriangleGPU>  m_Triangles;
		std::vector<RTBVHNodeGPU>   m_BVHNodes;
		std::vector<RTAABB>         m_TriAABBs;  // per-triangle bounding boxes
		std::vector<glm::vec3>      m_TriCentroids;

		Ref<StorageBuffer>          m_TriangleSSBO;
		Ref<StorageBuffer>          m_BVHSSBO;

		uint32_t m_TriangleCount    = 0;
		uint32_t m_BVHNodeCount     = 0;
		uint32_t m_TriSSBOCapacity  = 0;  // current SSBO capacity in bytes
		uint32_t m_BVHSSBOCapacity  = 0;
	};

} // namespace Lunex
