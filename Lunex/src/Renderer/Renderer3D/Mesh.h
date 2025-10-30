#pragma once

#include "Core/Core.h"
#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/Buffer.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Lunex {

	/**
	 * Vertex3D - Estructura de vértice para renderizado 3D
	 */
	struct Vertex3D {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
	};

	/**
	 * Mesh - Representa geometría 3D
	 * Contiene vertex array, vertex buffer e index buffer
	 */
	class Mesh {
	public:
		Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);
		~Mesh() = default;

		void Bind() const;
		void Unbind() const;

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }
		uint32_t GetIndexCount() const { return m_IndexCount; }
		uint32_t GetVertexCount() const { return m_VertexCount; }

		// Factory methods para crear meshes básicas
		static Ref<Mesh> CreateCube();
		static Ref<Mesh> CreateSphere(uint32_t segments = 32);
		static Ref<Mesh> CreatePlane();
		static Ref<Mesh> CreateQuad();

	private:
		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;

		// Bounding box (para frustum culling futuro)
		glm::vec3 m_BoundsMin = glm::vec3(0.0f);
		glm::vec3 m_BoundsMax = glm::vec3(0.0f);
	};

}
