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
		void SetEntityID(int entityID);

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		const std::vector<MeshTexture>& GetTextures() const { return m_Textures; }

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }

	private:
		void SetupMesh();

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<MeshTexture> m_Textures;

		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
	};

} // namespace Lunex
