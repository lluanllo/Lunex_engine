#include "stpch.h"
#include "RayTracingGeometry.h"

#include "Renderer/Model.h"
#include "Renderer/Mesh.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

namespace Lunex {
	
	// ============================================================================
	// RTTriangle Implementation
	// ============================================================================
	
	RTTriangle::RTTriangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, uint32_t matID) {
		v0 = glm::vec4(p0, static_cast<float>(matID));
		v1 = glm::vec4(p1, 0.0f);
		v2 = glm::vec4(p2, 0.0f);
		CalculateNormalAndArea();
	}
	
	void RTTriangle::CalculateNormalAndArea() {
		glm::vec3 edge1 = glm::vec3(v1) - glm::vec3(v0);
		glm::vec3 edge2 = glm::vec3(v2) - glm::vec3(v0);
		glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));
		float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
		normal = glm::vec4(n, area);
	}
	
	// ============================================================================
	// RTBVHNode Implementation
	// ============================================================================
	
	RTBVHNode::RTBVHNode() {
		aabbMin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, -1.0f);
		aabbMax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);
		firstTriangle = -1;
		rightChild = -1;
		parentNode = -1;
		padding = 0;
	}
	
	// ============================================================================
	// SceneGeometry Implementation
	// ============================================================================
	
	void SceneGeometry::Clear() {
		triangles.clear();
		sceneMin = glm::vec3(FLT_MAX);
		sceneMax = glm::vec3(-FLT_MAX);
	}
	
	void SceneGeometry::UpdateBounds(const glm::vec3& point) {
		sceneMin = glm::min(sceneMin, point);
		sceneMax = glm::max(sceneMax, point);
	}
	
	void SceneGeometry::Finalize() {
		// Ensure bounds are valid
		if (triangles.empty()) {
			sceneMin = glm::vec3(0.0f);
			sceneMax = glm::vec3(0.0f);
		}
		
		LNX_LOG_INFO("SceneGeometry: {0} triangles, bounds: [{1}, {2}, {3}] to [{4}, {5}, {6}]",
					 triangles.size(),
					 sceneMin.x, sceneMin.y, sceneMin.z,
					 sceneMax.x, sceneMax.y, sceneMax.z);
	}
	
	// ============================================================================
	// GeometryExtractor Implementation
	// ============================================================================
	
	SceneGeometry GeometryExtractor::ExtractFromScene(Scene* scene) {
		LNX_PROFILE_FUNCTION();
		
		SceneGeometry geometry;
		
		if (!scene) {
			LNX_LOG_ERROR("GeometryExtractor: Scene is null");
			return geometry;
		}
		
		// Extract from all mesh components
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		uint32_t materialID = 0;
		
		for (auto entityHandle : view) {
			Entity entity(entityHandle, scene);
			
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& meshComp = entity.GetComponent<MeshComponent>();
			
			if (meshComp.MeshModel) {
				glm::mat4 transformMatrix = transform.GetTransform();
				ExtractFromModel(meshComp.MeshModel.get(), transformMatrix, geometry, materialID++);
			}
		}
		
		geometry.Finalize();
		return geometry;
	}
	
	void GeometryExtractor::ExtractFromModel(const Model* model, const glm::mat4& transform,
											 SceneGeometry& outGeometry, uint32_t materialID) {
		if (!model) return;
		
		const auto& meshes = model->GetMeshes();
		for (const auto& mesh : meshes) {
			ExtractFromMesh(mesh.get(), transform, outGeometry, materialID);
		}
	}
	
	void GeometryExtractor::ExtractFromMesh(const Mesh* mesh, const glm::mat4& transform,
										   SceneGeometry& outGeometry, uint32_t materialID) {
		if (!mesh) return;
		
		const auto& vertices = mesh->GetVertices();
		const auto& indices = mesh->GetIndices();
		
		// Extract triangles
		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			uint32_t i0 = indices[i];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];
			
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
				LNX_LOG_WARN("GeometryExtractor: Invalid index");
				continue;
			}
			
			// Transform vertices to world space
			glm::vec3 v0 = glm::vec3(transform * glm::vec4(vertices[i0].Position, 1.0f));
			glm::vec3 v1 = glm::vec3(transform * glm::vec4(vertices[i1].Position, 1.0f));
			glm::vec3 v2 = glm::vec3(transform * glm::vec4(vertices[i2].Position, 1.0f));
			
			// Create triangle
			RTTriangle triangle(v0, v1, v2, materialID);
			outGeometry.triangles.push_back(triangle);
			
			// Update scene bounds
			outGeometry.UpdateBounds(v0);
			outGeometry.UpdateBounds(v1);
			outGeometry.UpdateBounds(v2);
		}
	}
}
