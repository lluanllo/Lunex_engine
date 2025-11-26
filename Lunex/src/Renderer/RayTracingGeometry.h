#pragma once

#include "Core/Core.h"
#include <glm/glm.hpp>
#include <vector>

namespace Lunex {
	
	// Triangle data matching compute shader layout
	struct RTTriangle {
		glm::vec4 v0; // xyz = position, w = materialID
		glm::vec4 v1;
		glm::vec4 v2;
		glm::vec4 normal; // xyz = face normal, w = area
		
		RTTriangle() = default;
		RTTriangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, uint32_t matID = 0);
		
		void CalculateNormalAndArea();
	};
	
	// BVH node matching compute shader layout
	struct RTBVHNode {
		glm::vec4 aabbMin; // xyz = min bounds, w = leftChild (or -1 if leaf)
		glm::vec4 aabbMax; // xyz = max bounds, w = triangleCount (if leaf)
		int32_t firstTriangle; // Index of first triangle (if leaf)
		int32_t rightChild;    // Index of right child
		int32_t parentNode;    // Index of parent node
		int32_t padding;
		
		RTBVHNode();
		bool IsLeaf() const { return aabbMax.w > 0; }
	};
	
	// Scene geometry data
	struct SceneGeometry {
		std::vector<RTTriangle> triangles;
		glm::vec3 sceneMin = glm::vec3(FLT_MAX);
		glm::vec3 sceneMax = glm::vec3(-FLT_MAX);
		
		void Clear();
		void UpdateBounds(const glm::vec3& point);
		void Finalize(); // Called after all triangles are added
	};
	
	// Geometry extractor - extracts triangles from models
	class GeometryExtractor {
	public:
		static SceneGeometry ExtractFromScene(class Scene* scene);
		static void ExtractFromModel(const class Model* model, const glm::mat4& transform, 
									 SceneGeometry& outGeometry, uint32_t materialID = 0);
		static void ExtractFromMesh(const class Mesh* mesh, const glm::mat4& transform,
									SceneGeometry& outGeometry, uint32_t materialID = 0);
	};
}
