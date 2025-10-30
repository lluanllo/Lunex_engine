#include "stpch.h"
#include "Mesh.h"

namespace Lunex {

	Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices) {
		m_VertexCount = static_cast<uint32_t>(vertices.size());
		m_IndexCount = static_cast<uint32_t>(indices.size());

		// Create Vertex Array
		m_VertexArray = VertexArray::Create();

		// Create Vertex Buffer
		m_VertexBuffer = VertexBuffer::Create((float*)vertices.data(), m_VertexCount * sizeof(Vertex3D));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Bitangent" }
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);

		// Create Index Buffer
		m_IndexBuffer = IndexBuffer::Create((uint32_t*)indices.data(), m_IndexCount);
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);

		// Calculate bounding box
		if (!vertices.empty()) {
			m_BoundsMin = m_BoundsMax = vertices[0].Position;
			for (const auto& v : vertices) {
				m_BoundsMin = glm::min(m_BoundsMin, v.Position);
				m_BoundsMax = glm::max(m_BoundsMax, v.Position);
			}
		}
	}

	void Mesh::Bind() const {
		m_VertexArray->Bind();
	}

	void Mesh::Unbind() const {
		m_VertexArray->Unbind();
	}

	// ============================================================================
	// Factory Methods - Primitivas básicas
	// ============================================================================

	Ref<Mesh> Mesh::CreateCube() {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		// Cube vertices (8 unique positions, but 24 vertices for proper normals)
		// Front face (+Z)
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });

		// Back face (-Z)
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });

		// Top face (+Y)
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });

		// Bottom face (-Y)
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });

		// Right face (+X)
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });

		// Left face (-X)
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });

		// Indices (6 faces * 2 triangles * 3 indices = 36)
		uint32_t faceIndices[] = { 0, 1, 2, 2, 3, 0 }; // CCW winding
		for (uint32_t face = 0; face < 6; face++) {
			for (uint32_t i = 0; i < 6; i++) {
				indices.push_back(face * 4 + faceIndices[i]);
			}
		}

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> Mesh::CreatePlane() {
		std::vector<Vertex3D> vertices = {
			{ {-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ { 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ { 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ {-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} }
		};

		std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> Mesh::CreateQuad() {
		std::vector<Vertex3D> vertices = {
			{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
			{ { 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
			{ { 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
			{ {-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} }
		};

		std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> Mesh::CreateSphere(uint32_t segments) {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		// Simple UV sphere generation
		const float radius = 0.5f;

		for (uint32_t lat = 0; lat <= segments; lat++) {
			float theta = lat * glm::pi<float>() / segments;
			float sinTheta = glm::sin(theta);
			float cosTheta = glm::cos(theta);

			for (uint32_t lon = 0; lon <= segments; lon++) {
				float phi = lon * 2.0f * glm::pi<float>() / segments;
				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				Vertex3D vertex;
				vertex.Position = glm::vec3(
					radius * sinTheta * cosPhi,
					radius * cosTheta,
					radius * sinTheta * sinPhi
				);
				vertex.Normal = glm::normalize(vertex.Position);
				vertex.TexCoord = glm::vec2((float)lon / segments, (float)lat / segments);
				vertex.Tangent = glm::vec3(-sinPhi, 0.0f, cosPhi);
				vertex.Bitangent = glm::cross(vertex.Normal, vertex.Tangent);

				vertices.push_back(vertex);
			}
		}

		// Generate indices
		for (uint32_t lat = 0; lat < segments; lat++) {
			for (uint32_t lon = 0; lon < segments; lon++) {
				uint32_t first = (lat * (segments + 1)) + lon;
				uint32_t second = first + segments + 1;

				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		return CreateRef<Mesh>(vertices, indices);
	}

}
