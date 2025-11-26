#include "stpch.h"
#include "BVHBuilder.h"
#include <chrono>
#include <intrin.h>  // Para _BitScanReverse en MSVC

namespace Lunex {
	
	// ============================================================================
	// PUBLIC API
	// ============================================================================
	
	std::vector<RTBVHNode> BVHBuilder::Build(SceneGeometry& geometry) {
		LNX_PROFILE_FUNCTION();
		
		auto startTime = std::chrono::high_resolution_clock::now();
		m_Stats = Stats();
		
		std::vector<RTBVHNode> nodes;
		
		if (geometry.triangles.empty()) {
			LNX_LOG_WARN("BVHBuilder: No triangles to build BVH");
			return nodes;
		}
		
		LNX_LOG_INFO("BVHBuilder: Building BVH for {0} triangles", geometry.triangles.size());
		
		// Step 1: Calculate Morton codes for each triangle centroid
		std::vector<uint32_t> mortonCodes(geometry.triangles.size());
		std::vector<uint32_t> triangleIndices(geometry.triangles.size());
		
		for (size_t i = 0; i < geometry.triangles.size(); i++) {
			glm::vec3 centroid = GetTriangleCentroid(geometry.triangles[i]);
			mortonCodes[i] = CalculateMortonCode(centroid, geometry.sceneMin, geometry.sceneMax);
			triangleIndices[i] = static_cast<uint32_t>(i);
		}
		
		// Step 2: Sort triangles by Morton code
		RadixSort(mortonCodes, triangleIndices);
		
		// Step 3: Build tree hierarchy
		// Allocate space for nodes (worst case: 2N-1 nodes for N triangles)
		nodes.reserve(geometry.triangles.size() * 2);
		
		// Build recursively
		BuildRecursive(nodes, geometry, triangleIndices, 0, 
					   static_cast<int>(geometry.triangles.size()), -1);
		
		// Calculate statistics
		auto endTime = std::chrono::high_resolution_clock::now();
		m_Stats.nodeCount = static_cast<uint32_t>(nodes.size());
		m_Stats.buildTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		
		for (const auto& node : nodes) {
			if (node.IsLeaf()) {
				m_Stats.leafCount++;
			} else {
				m_Stats.internalCount++;
			}
		}
		
		LNX_LOG_INFO("BVH Built: {0} nodes ({1} leaves, {2} internal) in {3:.2f}ms",
					 m_Stats.nodeCount, m_Stats.leafCount, m_Stats.internalCount, m_Stats.buildTimeMs);
		
		return nodes;
	}
	
	// ============================================================================
	// MORTON CODE GENERATION
	// ============================================================================
	
	uint32_t BVHBuilder::ExpandBits(uint32_t v) {
		v = (v * 0x00010001u) & 0xFF0000FFu;
		v = (v * 0x00000101u) & 0x0F00F00Fu;
		v = (v * 0x00000011u) & 0xC30C30C3u;
		v = (v * 0x00000005u) & 0x49249249u;
		return v;
	}
	
	uint32_t BVHBuilder::CalculateMortonCode(const glm::vec3& position, 
											 const glm::vec3& sceneMin, 
											 const glm::vec3& sceneMax) {
		// Normalize position to [0, 1024)
		glm::vec3 extent = sceneMax - sceneMin;
		glm::vec3 normalized = (position - sceneMin) / glm::max(extent, glm::vec3(0.0001f));
		normalized = glm::clamp(normalized * 1024.0f, glm::vec3(0.0f), glm::vec3(1023.0f));
		
		uint32_t x = ExpandBits(static_cast<uint32_t>(normalized.x));
		uint32_t y = ExpandBits(static_cast<uint32_t>(normalized.y));
		uint32_t z = ExpandBits(static_cast<uint32_t>(normalized.z));
		
		return (x << 2) | (y << 1) | z;
	}
	
	// ============================================================================
	// RADIX SORT
	// ============================================================================
	
	void BVHBuilder::RadixSort(std::vector<uint32_t>& keys, std::vector<uint32_t>& values) {
		LNX_PROFILE_FUNCTION();
		
		// Simple radix sort implementation
		const int BITS = 32;
		const int RADIX_BITS = 8;
		const int RADIX = 1 << RADIX_BITS;
		const int MASK = RADIX - 1;
		
		std::vector<uint32_t> tempKeys(keys.size());
		std::vector<uint32_t> tempValues(values.size());
		
		for (int shift = 0; shift < BITS; shift += RADIX_BITS) {
			// Count occurrences
			std::vector<int> count(RADIX, 0);
			for (size_t i = 0; i < keys.size(); i++) {
				int bucket = (keys[i] >> shift) & MASK;
				count[bucket]++;
			}
			
			// Compute prefix sum
			std::vector<int> offset(RADIX, 0);
			for (int i = 1; i < RADIX; i++) {
				offset[i] = offset[i - 1] + count[i - 1];
			}
			
			// Scatter
			for (size_t i = 0; i < keys.size(); i++) {
				int bucket = (keys[i] >> shift) & MASK;
				int dstIndex = offset[bucket]++;
				tempKeys[dstIndex] = keys[i];
				tempValues[dstIndex] = values[i];
			}
			
			// Swap buffers
			std::swap(keys, tempKeys);
			std::swap(values, tempValues);
		}
	}
	
	// ============================================================================
	// BVH TREE CONSTRUCTION
	// ============================================================================
	
	int BVHBuilder::FindSplit(const std::vector<uint32_t>& mortonCodes, int start, int end) {
		// Find split position using common prefix
		uint32_t firstCode = mortonCodes[start];
		uint32_t lastCode = mortonCodes[end - 1];
		
		// Identical codes ? split in the middle
		if (firstCode == lastCode) {
			return (start + end) / 2;
		}
		
		// Calculate common prefix length using bit manipulation
		// Count leading zeros to find first differing bit
		uint32_t xorValue = firstCode ^ lastCode;
		
		// Count leading zeros (CLZ)
		int commonPrefix = 0;
		#ifdef _MSC_VER
			// MSVC intrinsic
			unsigned long index;
			_BitScanReverse(&index, xorValue);
			commonPrefix = 31 - index;
		#else
			// GCC/Clang intrinsic
			commonPrefix = __builtin_clz(xorValue);
		#endif
		
		// Binary search for split position
		int split = start;
		int step = end - start;
		
		do {
			step = (step + 1) >> 1;
			int newSplit = split + step;
			
			if (newSplit < end) {
				uint32_t splitCode = mortonCodes[newSplit];
				
				// Calculate prefix with split code
				uint32_t splitXor = firstCode ^ splitCode;
				int splitPrefix = 0;
				
				#ifdef _MSC_VER
					unsigned long splitIndex;
					_BitScanReverse(&splitIndex, splitXor);
					splitPrefix = 31 - splitIndex;
				#else
					splitPrefix = __builtin_clz(splitXor);
				#endif
				
				if (splitPrefix > commonPrefix) {
					split = newSplit;
				}
			}
		} while (step > 1);
		
		return split + 1;
	}
	
	int BVHBuilder::BuildRecursive(std::vector<RTBVHNode>& nodes,
								   SceneGeometry& geometry,
								   const std::vector<uint32_t>& sortedIndices,
								   int start, int end, int parentIndex) {
		if (start >= end) return -1;
		
		int nodeIndex = static_cast<int>(nodes.size());
		nodes.emplace_back();
		RTBVHNode& node = nodes[nodeIndex];
		
		node.parentNode = parentIndex;
		
		int count = end - start;
		
		// Leaf node (small number of triangles)
		if (count <= 4) {
			// Create leaf
			node.firstTriangle = start;
			node.aabbMax.w = static_cast<float>(count); // Store triangle count
			node.aabbMin.w = -1.0f; // Mark as leaf
			
			// Calculate AABB
			glm::vec3 min, max;
			CalculateAABB(geometry, sortedIndices, start, end, min, max);
			node.aabbMin = glm::vec4(min, -1.0f);
			node.aabbMax = glm::vec4(max, static_cast<float>(count));
			
			return nodeIndex;
		}
		
		// Internal node - find split
		int split = FindSplit(const_cast<std::vector<uint32_t>&>(sortedIndices), start, end);
		
		// Build children
		int leftChild = BuildRecursive(nodes, geometry, sortedIndices, start, split, nodeIndex);
		int rightChild = BuildRecursive(nodes, geometry, sortedIndices, split, end, nodeIndex);
		
		// Update node (might have been reallocated)
		RTBVHNode& currentNode = nodes[nodeIndex];
		currentNode.aabbMin.w = static_cast<float>(leftChild);
		currentNode.rightChild = rightChild;
		
		// Calculate AABB as union of children
		if (leftChild >= 0 && rightChild >= 0) {
			glm::vec3 leftMin = glm::vec3(nodes[leftChild].aabbMin);
			glm::vec3 leftMax = glm::vec3(nodes[leftChild].aabbMax);
			glm::vec3 rightMin = glm::vec3(nodes[rightChild].aabbMin);
			glm::vec3 rightMax = glm::vec3(nodes[rightChild].aabbMax);
			
			glm::vec3 minBounds = glm::min(leftMin, rightMin);
			glm::vec3 maxBounds = glm::max(leftMax, rightMax);
			
			currentNode.aabbMin = glm::vec4(minBounds, static_cast<float>(leftChild));
			currentNode.aabbMax = glm::vec4(maxBounds, 0.0f); // 0 = internal node
		}
		
		return nodeIndex;
	}
	
	// ============================================================================
	// HELPER FUNCTIONS
	// ============================================================================
	
	void BVHBuilder::CalculateAABB(const SceneGeometry& geometry,
								   const std::vector<uint32_t>& indices,
								   int start, int end,
								   glm::vec3& outMin, glm::vec3& outMax) {
		outMin = glm::vec3(FLT_MAX);
		outMax = glm::vec3(-FLT_MAX);
		
		for (int i = start; i < end; i++) {
			uint32_t triIndex = indices[i];
			if (triIndex >= geometry.triangles.size()) continue;
			
			const RTTriangle& tri = geometry.triangles[triIndex];
			
			outMin = glm::min(outMin, glm::vec3(tri.v0));
			outMin = glm::min(outMin, glm::vec3(tri.v1));
			outMin = glm::min(outMin, glm::vec3(tri.v2));
			
			outMax = glm::max(outMax, glm::vec3(tri.v0));
			outMax = glm::max(outMax, glm::vec3(tri.v1));
			outMax = glm::max(outMax, glm::vec3(tri.v2));
		}
	}
	
	glm::vec3 BVHBuilder::GetTriangleCentroid(const RTTriangle& tri) {
		return (glm::vec3(tri.v0) + glm::vec3(tri.v1) + glm::vec3(tri.v2)) / 3.0f;
	}
}
