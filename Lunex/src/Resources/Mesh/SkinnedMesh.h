#pragma once

/**
 * @file SkinnedMesh.h
 * @brief Skinned mesh vertex format and data structures
 * 
 * Part of the AAA Animation System
 * 
 * Extends the standard Mesh with bone weight data for skeletal animation.
 */

#include "Mesh.h"
#include "Core/Core.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	// Maximum bones influencing a single vertex
	constexpr int MAX_BONE_INFLUENCE = 4;

	/**
	 * @struct SkinnedVertex
	 * @brief Vertex format with bone weights for skeletal animation
	 */
	struct SkinnedVertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
		int EntityID = -1;
		
		// ? Bone data for skinning
		glm::ivec4 BoneIDs = glm::ivec4(-1);       // Bone indices (up to 4)
		glm::vec4 BoneWeights = glm::vec4(0.0f);   // Bone weights (sum should = 1)
		
		// Add bone influence
		void AddBoneInfluence(int boneID, float weight) {
			// Find an empty slot
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				if (BoneIDs[i] < 0) {
					BoneIDs[i] = boneID;
					BoneWeights[i] = weight;
					return;
				}
			}
			
			// If all slots are full, replace the smallest weight
			int minIndex = 0;
			float minWeight = BoneWeights[0];
			for (int i = 1; i < MAX_BONE_INFLUENCE; i++) {
				if (BoneWeights[i] < minWeight) {
					minWeight = BoneWeights[i];
					minIndex = i;
				}
			}
			
			if (weight > minWeight) {
				BoneIDs[minIndex] = boneID;
				BoneWeights[minIndex] = weight;
			}
		}
		
		// Normalize bone weights to sum to 1
		void NormalizeBoneWeights() {
			float total = BoneWeights.x + BoneWeights.y + BoneWeights.z + BoneWeights.w;
			if (total > 0.0f) {
				BoneWeights /= total;
			}
		}
	};

	/**
	 * @class SkinnedMesh
	 * @brief GPU-ready skinned mesh with bone weights
	 */
	class SkinnedMesh {
	public:
		SkinnedMesh(const std::vector<SkinnedVertex>& vertices,
					const std::vector<uint32_t>& indices,
					const std::vector<MeshTexture>& textures = {});
		~SkinnedMesh() = default;

		void Draw(const Ref<Shader>& shader);
		void SetEntityID(int entityID);

		const std::vector<SkinnedVertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		const std::vector<MeshTexture>& GetTextures() const { return m_Textures; }

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }
		
		// Check if mesh has valid bone weights
		bool HasBoneWeights() const { return m_HasBoneWeights; }

	private:
		void SetupMesh();

	private:
		std::vector<SkinnedVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<MeshTexture> m_Textures;

		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		
		bool m_HasBoneWeights = false;
	};

	/**
	 * @brief Convert standard vertices to skinned vertices
	 */
	inline std::vector<SkinnedVertex> ConvertToSkinnedVertices(const std::vector<Vertex>& vertices) {
		std::vector<SkinnedVertex> skinnedVerts;
		skinnedVerts.reserve(vertices.size());
		
		for (const auto& v : vertices) {
			SkinnedVertex sv;
			sv.Position = v.Position;
			sv.Normal = v.Normal;
			sv.TexCoords = v.TexCoords;
			sv.Tangent = v.Tangent;
			sv.Bitangent = v.Bitangent;
			sv.EntityID = v.EntityID;
			sv.BoneIDs = glm::ivec4(-1);
			sv.BoneWeights = glm::vec4(0.0f);
			skinnedVerts.push_back(sv);
		}
		
		return skinnedVerts;
	}

}
