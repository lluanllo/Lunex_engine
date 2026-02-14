#include "stpch.h"
#include "MeshGeometryPool.h"

#include "Resources/Mesh/Mesh.h"
#include "Core/JobSystem/JobSystem.h"
#include "Log/Log.h"

#include <algorithm>
#include <numeric>
#include <chrono>

namespace Lunex {

	// ====================================================================
	// CONSTANTS
	// ====================================================================

	static constexpr int   BVH_MAX_DEPTH      = 32;
	static constexpr int   BVH_LEAF_MAX_TRIS  = 4;
	static constexpr int   SAH_BUCKETS         = 12;
	static constexpr float SAH_TRAVERSAL_COST  = 1.0f;
	static constexpr float SAH_INTERSECT_COST  = 1.5f;

	// Threshold for parallelization — below this count, single-threaded is faster
	static constexpr uint32_t PARALLEL_AABB_THRESHOLD = 1024;

	// ====================================================================
	// LIFECYCLE
	// ====================================================================

	void MeshGeometryPool::Initialize() {
		// SSBOs created on first Build()
	}

	void MeshGeometryPool::Shutdown() {
		m_TriangleSSBO.reset();
		m_BVHSSBO.reset();
		m_Triangles.clear();
		m_BVHNodes.clear();
		m_TriAABBs.clear();
		m_TriCentroids.clear();
		m_TriSSBOCapacity  = 0;
		m_BVHSSBOCapacity  = 0;
	}

	// ====================================================================
	// BUILD — orchestrates the 4 steps
	// ====================================================================

	MeshPoolBuildResult MeshGeometryPool::Build(
		const std::vector<SceneDrawItem>& items,
		std::function<uint32_t(const Ref<MaterialInstance>&)> getMaterialIndex)
	{
		auto t0 = std::chrono::high_resolution_clock::now();

		m_Triangles.clear();
		m_BVHNodes.clear();
		m_TriAABBs.clear();
		m_TriCentroids.clear();

		// Step 1: Flatten all meshes into world-space triangles
		FlattenMeshes(items, std::move(getMaterialIndex));

		m_TriangleCount = static_cast<uint32_t>(m_Triangles.size());

		if (m_TriangleCount == 0) {
			m_BVHNodeCount = 0;
			return { 0, 0, 0.0f };
		}

		// Step 2: Compute per-triangle AABBs (parallel via JobSystem)
		ComputeAABBsParallel();

		// Step 3: Build BVH with SAH
		BuildBVH();

		// Step 4: Upload to GPU
		UploadToGPU();

		auto t1 = std::chrono::high_resolution_clock::now();
		float buildMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

		MeshPoolBuildResult result;
		result.TriangleCount = m_TriangleCount;
		result.BVHNodeCount  = m_BVHNodeCount;
		result.BuildTimeMs   = buildMs;
		return result;
	}

	void MeshGeometryPool::BindForRayTracing(uint32_t triangleBinding, uint32_t bvhBinding) const {
		if (m_TriangleSSBO) m_TriangleSSBO->BindForCompute(triangleBinding);
		if (m_BVHSSBO)     m_BVHSSBO->BindForCompute(bvhBinding);
	}

	// ====================================================================
	// STEP 1: FLATTEN MESHES
	// ====================================================================

	void MeshGeometryPool::FlattenMeshes(
		const std::vector<SceneDrawItem>& items,
		std::function<uint32_t(const Ref<MaterialInstance>&)> getMaterialIndex)
	{
		// Pre-estimate capacity to avoid reallocation
		size_t estimatedTris = 0;
		for (const auto& item : items) {
			if (!item.MeshModel) continue;
			for (const auto& mesh : item.MeshModel->GetMeshes())
				estimatedTris += mesh->GetIndices().size() / 3;
		}
		m_Triangles.reserve(estimatedTris);

		for (const auto& item : items) {
			if (!item.MeshModel) continue;

			uint32_t matIdx = getMaterialIndex(item.Material);
			float entityF   = static_cast<float>(item.EntityID);

			glm::mat4 model   = item.Transform;
			glm::mat3 normalM = glm::mat3(glm::transpose(glm::inverse(model)));

			for (const auto& mesh : item.MeshModel->GetMeshes()) {
				const auto& verts   = mesh->GetVertices();
				const auto& indices = mesh->GetIndices();

				for (size_t i = 0; i + 2 < indices.size(); i += 3) {
					const auto& v0 = verts[indices[i + 0]];
					const auto& v1 = verts[indices[i + 1]];
					const auto& v2 = verts[indices[i + 2]];

					glm::vec3 p0 = glm::vec3(model * glm::vec4(v0.Position, 1.0f));
					glm::vec3 p1 = glm::vec3(model * glm::vec4(v1.Position, 1.0f));
					glm::vec3 p2 = glm::vec3(model * glm::vec4(v2.Position, 1.0f));

					glm::vec3 n0 = glm::normalize(normalM * v0.Normal);
					glm::vec3 n1 = glm::normalize(normalM * v1.Normal);
					glm::vec3 n2 = glm::normalize(normalM * v2.Normal);

					glm::vec3 t0 = glm::normalize(normalM * v0.Tangent);

					RTTriangleGPU tri;
					tri.V0             = glm::vec4(p0, static_cast<float>(matIdx));
					tri.V1             = glm::vec4(p1, entityF);
					tri.V2             = glm::vec4(p2, 0.0f);
					tri.TexCoords01    = glm::vec4(v0.TexCoords, v1.TexCoords);
					tri.TexCoords2AndMat = glm::vec4(v2.TexCoords, 0.0f, 0.0f);
					tri.N0N1           = glm::vec4(n0, n1.x);
					tri.N1N2           = glm::vec4(n1.y, n1.z, n2.x, n2.y);
					tri.N2T0           = glm::vec4(n2.z, t0);

					m_Triangles.push_back(tri);
				}
			}
		}
	}

	// ====================================================================
	// STEP 2: COMPUTE AABBS IN PARALLEL (JobSystem)
	// ====================================================================

	void MeshGeometryPool::ComputeAABBsParallel() {
		const uint32_t count = m_TriangleCount;
		m_TriAABBs.resize(count);
		m_TriCentroids.resize(count);

		if (count < PARALLEL_AABB_THRESHOLD) {
			// Single-threaded path for small counts
			for (uint32_t i = 0; i < count; ++i) {
				const auto& tri = m_Triangles[i];
				glm::vec3 p0 = glm::vec3(tri.V0);
				glm::vec3 p1 = glm::vec3(tri.V1);
				glm::vec3 p2 = glm::vec3(tri.V2);

				RTAABB aabb;
				aabb.Expand(p0);
				aabb.Expand(p1);
				aabb.Expand(p2);
				m_TriAABBs[i]    = aabb;
				m_TriCentroids[i] = aabb.Center();
			}
			return;
		}

		// Parallel path: use JobSystem::ParallelFor
		// Each iteration is independent (write to own index), no synchronization needed.
		auto counter = JobSystem::Get().ParallelFor(
			0u, count,
			[this](uint32_t i) {
				const auto& tri = m_Triangles[i];
				glm::vec3 p0 = glm::vec3(tri.V0);
				glm::vec3 p1 = glm::vec3(tri.V1);
				glm::vec3 p2 = glm::vec3(tri.V2);

				RTAABB aabb;
				aabb.Expand(p0);
				aabb.Expand(p1);
				aabb.Expand(p2);
				m_TriAABBs[i]    = aabb;
				m_TriCentroids[i] = aabb.Center();
			},
			0,  // auto grain size
			JobPriority::High
		);

		// Wait for all AABB jobs to complete before building BVH
		JobSystem::Get().Wait(counter);
	}

	// ====================================================================
	// STEP 3: BVH CONSTRUCTION (SAH)
	// ====================================================================

	void MeshGeometryPool::BuildBVH() {
		// Reserve worst case: 2 * N nodes
		m_BVHNodes.clear();
		m_BVHNodes.resize(2 * m_TriangleCount);

		// Root node encompasses all triangles
		RTBVHNodeGPU& root = m_BVHNodes[0];
		RTAABB rootBounds;
		for (uint32_t i = 0; i < m_TriangleCount; ++i)
			rootBounds.Expand(m_TriAABBs[i]);

		root.BoundsMin = glm::vec4(rootBounds.Min, 0.0f);
		root.BoundsMax = glm::vec4(rootBounds.Max, static_cast<float>(m_TriangleCount));

		m_BVHNodeCount = 1;
		BuildBVHRecursive(0, 0, m_TriangleCount, 0);

		// Shrink to actual size
		m_BVHNodes.resize(m_BVHNodeCount);
	}

	void MeshGeometryPool::BuildBVHRecursive(uint32_t nodeIdx, uint32_t start, uint32_t end, int depth) {
		uint32_t count = end - start;

		// Leaf condition
		if (count <= BVH_LEAF_MAX_TRIS || depth >= BVH_MAX_DEPTH) {
			auto& node = m_BVHNodes[nodeIdx];
			node.BoundsMin.w = static_cast<float>(start);    // triStart
			node.BoundsMax.w = static_cast<float>(count);    // triCount > 0 = leaf
			return;
		}

		// Compute bounds of centroids
		RTAABB centroidBounds;
		for (uint32_t i = start; i < end; ++i)
			centroidBounds.Expand(m_TriCentroids[i]);

		glm::vec3 ext = centroidBounds.Extent();
		int axis = (ext.x >= ext.y && ext.x >= ext.z) ? 0 : (ext.y >= ext.z ? 1 : 2);

		float axisMin = centroidBounds.Min[axis];
		float axisMax = centroidBounds.Max[axis];

		if (axisMax - axisMin < 1e-6f) {
			// Degenerate — make leaf
			auto& node = m_BVHNodes[nodeIdx];
			node.BoundsMin.w = static_cast<float>(start);
			node.BoundsMax.w = static_cast<float>(count);
			return;
		}

		// SAH bucketed evaluation
		float bestCost    = std::numeric_limits<float>::max();
		int   bestBucket  = -1;
		float parentArea  = RTAABB{glm::vec3(m_BVHNodes[nodeIdx].BoundsMin),
		                           glm::vec3(m_BVHNodes[nodeIdx].BoundsMax)}.SurfaceArea();

		// Bucket counts and bounds
		struct Bucket { uint32_t count = 0; RTAABB bounds; };
		Bucket buckets[SAH_BUCKETS];

		float scale = static_cast<float>(SAH_BUCKETS) / (axisMax - axisMin);
		for (uint32_t i = start; i < end; ++i) {
			int b = std::min(static_cast<int>((m_TriCentroids[i][axis] - axisMin) * scale),
			                 SAH_BUCKETS - 1);
			buckets[b].count++;
			buckets[b].bounds.Expand(m_TriAABBs[i]);
		}

		// Sweep from left — accumulate left bounds & counts
		RTAABB leftAccum;
		uint32_t leftCount = 0;
		float leftAreas[SAH_BUCKETS - 1];
		uint32_t leftCounts[SAH_BUCKETS - 1];
		for (int i = 0; i < SAH_BUCKETS - 1; ++i) {
			leftAccum.Expand(buckets[i].bounds);
			leftCount += buckets[i].count;
			leftAreas[i]  = leftAccum.IsValid() ? leftAccum.SurfaceArea() : 0.0f;
			leftCounts[i] = leftCount;
		}

		// Sweep from right and evaluate cost at each split
		RTAABB rightAccum;
		uint32_t rightCount = 0;
		for (int i = SAH_BUCKETS - 1; i > 0; --i) {
			rightAccum.Expand(buckets[i].bounds);
			rightCount += buckets[i].count;

			float rightArea = rightAccum.IsValid() ? rightAccum.SurfaceArea() : 0.0f;
			float cost = SAH_TRAVERSAL_COST
			           + SAH_INTERSECT_COST * (leftCounts[i - 1] * leftAreas[i - 1]
			                                 + rightCount * rightArea) / parentArea;
			if (cost < bestCost) {
				bestCost   = cost;
				bestBucket = i;
			}
		}

		// If no split improves over leaf cost, make leaf
		float leafCost = SAH_INTERSECT_COST * static_cast<float>(count);
		if (bestBucket < 0 || bestCost >= leafCost) {
			auto& node = m_BVHNodes[nodeIdx];
			node.BoundsMin.w = static_cast<float>(start);
			node.BoundsMax.w = static_cast<float>(count);
			return;
		}

		// Partition triangles around the best bucket split.
		// All arrays (Triangles, TriAABBs, TriCentroids) must be swapped together.
		float splitPos = axisMin + static_cast<float>(bestBucket) / scale;

		uint32_t midIdx = start;
		{
			uint32_t left = start, right = end - 1;
			while (left <= right && right < end) {
				if (m_TriCentroids[left][axis] < splitPos) {
					left++;
				} else {
					std::swap(m_Triangles[left],    m_Triangles[right]);
					std::swap(m_TriAABBs[left],     m_TriAABBs[right]);
					std::swap(m_TriCentroids[left],  m_TriCentroids[right]);
					right--;
				}
			}
			midIdx = left;
		}

		// Avoid empty partitions (fallback to midpoint split)
		if (midIdx == start || midIdx == end) {
			midIdx = start + count / 2;
		}

		// Create child nodes
		uint32_t leftChildIdx  = m_BVHNodeCount++;
		uint32_t rightChildIdx = m_BVHNodeCount++;

		// Compute child bounds
		RTAABB leftBounds, rightBounds;
		for (uint32_t i = start; i < midIdx; ++i)   leftBounds.Expand(m_TriAABBs[i]);
		for (uint32_t i = midIdx; i < end;   ++i)   rightBounds.Expand(m_TriAABBs[i]);

		m_BVHNodes[leftChildIdx].BoundsMin  = glm::vec4(leftBounds.Min,  0.0f);
		m_BVHNodes[leftChildIdx].BoundsMax  = glm::vec4(leftBounds.Max,  0.0f);
		m_BVHNodes[rightChildIdx].BoundsMin = glm::vec4(rightBounds.Min, 0.0f);
		m_BVHNodes[rightChildIdx].BoundsMax = glm::vec4(rightBounds.Max, 0.0f);

		// Mark parent as internal: triCount = 0, leftChild in BoundsMin.w
		auto& node = m_BVHNodes[nodeIdx];
		node.BoundsMin.w = static_cast<float>(leftChildIdx);
		node.BoundsMax.w = 0.0f;  // 0 = internal node

		// Recurse
		BuildBVHRecursive(leftChildIdx,  start,  midIdx, depth + 1);
		BuildBVHRecursive(rightChildIdx, midIdx, end,    depth + 1);
	}

	// ====================================================================
	// STEP 4: UPLOAD TO GPU
	// ====================================================================

	void MeshGeometryPool::UploadToGPU() {
		// Triangles
		{
			uint32_t triBytes = m_TriangleCount * sizeof(RTTriangleGPU);
			if (!m_TriangleSSBO || triBytes > m_TriSSBOCapacity) {
				uint32_t newCap = std::max(triBytes * 2, 1024u);
				m_TriangleSSBO = StorageBuffer::Create(newCap, 0);
				m_TriSSBOCapacity = newCap;
			}
			m_TriangleSSBO->SetData(m_Triangles.data(), triBytes);
		}
		// BVH Nodes
		{
			uint32_t bvhBytes = m_BVHNodeCount * sizeof(RTBVHNodeGPU);
			if (!m_BVHSSBO || bvhBytes > m_BVHSSBOCapacity) {
				uint32_t newCap = std::max(bvhBytes * 2, 1024u);
				m_BVHSSBO = StorageBuffer::Create(newCap, 0);
				m_BVHSSBOCapacity = newCap;
			}
			m_BVHSSBO->SetData(m_BVHNodes.data(), bvhBytes);
		}
	}

} // namespace Lunex
