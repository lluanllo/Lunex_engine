#pragma once

/**
 * @file Mesh.h
 * @brief GPU mesh resource
 *
 * AAA Architecture: Mesh lives in Resources/Mesh/
 * This is the GPU-ready mesh with vertex/index buffers.
 */

#include "Core/Core.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Lunex {

	/**
	 * @struct Vertex
	 * @brief Standard vertex format
	 */
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
		int EntityID = -1;
	};

	/**
	 * @struct MeshTexture
	 * @brief Texture reference for mesh rendering
	 */
	struct MeshTexture {
		Ref<Texture2D> Texture;
		std::string Type;
		std::string Path;
	};

	/**
	 * @class Mesh
	 * @brief GPU-ready mesh with vertex/index buffers
	 */
	class Mesh {
	public:
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
			const std::vector<MeshTexture>& textures = {});
		~Mesh() = default;

		void Draw(const Ref<Shader>& shader);
		// Draw without binding the imported textures (graph materials bind their own)
		void DrawGeometry();
		void SetEntityID(int entityID);

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		const std::vector<MeshTexture>& GetTextures() const { return m_Textures; }

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }

		// Local-space bounding box (computed once at creation)
		const glm::vec3& GetAABBMin() const { return m_AABBMin; }
		const glm::vec3& GetAABBMax() const { return m_AABBMax; }

		// Skinning data (second vertex buffer, attribute locations 6 = bone ids, 7 = weights)
		void SetBoneData(const std::vector<glm::ivec4>& boneIDs, const std::vector<glm::vec4>& boneWeights);
		bool IsSkinned() const { return m_Skinned; }
		// CPU copy of the skinning data (backends that upload their own buffers, e.g. Vulkan)
		const std::vector<glm::ivec4>& GetBoneIDs() const { return m_BoneIDs; }
		const std::vector<glm::vec4>& GetBoneWeights() const { return m_BoneWeights; }

		// Transforms a local AABB by a matrix, returning the enclosing world AABB (Arvo's method)
		static void TransformAABB(const glm::mat4& m, const glm::vec3& localMin, const glm::vec3& localMax,
			glm::vec3& outMin, glm::vec3& outMax);

	private:
		void SetupMesh();

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<MeshTexture> m_Textures;

		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		Ref<VertexBuffer> m_BoneBuffer;
		std::vector<glm::ivec4> m_BoneIDs;
		std::vector<glm::vec4> m_BoneWeights;
		bool m_Skinned = false;

		glm::vec3 m_AABBMin = glm::vec3(0.0f);
		glm::vec3 m_AABBMax = glm::vec3(0.0f);

		// Sampler uniform names ("texture_diffuse1", ...) resolved once instead of every draw
		std::vector<std::string> m_TextureUniformNames;
	};

} // namespace Lunex