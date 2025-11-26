#pragma once

#include "Core/Core.h"
#include "RayTracingGeometry.h"
#include <vector>
#include <algorithm>

namespace Lunex {
	
	// BVH Builder using LBVH (Linear Bounding Volume Hierarchy) algorithm
	class BVHBuilder {
	public:
		BVHBuilder() = default;
		~BVHBuilder() = default;
		
		// Build BVH from scene geometry
		std::vector<RTBVHNode> Build(SceneGeometry& geometry);
		
		// Get statistics
		struct Stats {
			uint32_t nodeCount = 0;
			uint32_t leafCount = 0;
			uint32_t internalCount = 0;
			float buildTimeMs = 0.0f;
		};
		
		const Stats& GetStats() const { return m_Stats; }
		
	private:
		// Morton code generation
		uint32_t CalculateMortonCode(const glm::vec3& position, const glm::vec3& sceneMin, const glm::vec3& sceneMax);
		uint32_t ExpandBits(uint32_t v);
		
		// Radix sort for Morton codes
		void RadixSort(std::vector<uint32_t>& keys, std::vector<uint32_t>& values);
		
		// Build tree structure
		int BuildRecursive(std::vector<RTBVHNode>& nodes, 
						   SceneGeometry& geometry,
						   const std::vector<uint32_t>& sortedIndices,
						   int start, int end, int parentIndex);
		
		// AABB calculations
		void CalculateAABB(const SceneGeometry& geometry, 
						   const std::vector<uint32_t>& indices,
						   int start, int end,
						   glm::vec3& outMin, glm::vec3& outMax);
		
		// Helper functions
		glm::vec3 GetTriangleCentroid(const RTTriangle& tri);
		int FindSplit(const std::vector<uint32_t>& mortonCodes, int start, int end);
		
		Stats m_Stats;
	};
}
