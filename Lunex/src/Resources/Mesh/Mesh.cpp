#include "stpch.h"
#include "Mesh.h"

#include "RHI/RHI.h"

namespace Lunex {

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<MeshTexture>& textures)
		: m_Vertices(vertices), m_Indices(indices), m_Textures(textures)
	{
		SetupMesh();
	}

	void Mesh::SetupMesh() {
		// Bounding box
		if (!m_Vertices.empty()) {
			m_AABBMin = m_AABBMax = m_Vertices[0].Position;
			for (const auto& v : m_Vertices) {
				m_AABBMin = glm::min(m_AABBMin, v.Position);
				m_AABBMax = glm::max(m_AABBMax, v.Position);
			}
		}

		// Sampler uniform names
		uint32_t diffuseNr = 1, specularNr = 1, normalNr = 1, heightNr = 1;
		m_TextureUniformNames.reserve(m_Textures.size());
		for (const auto& tex : m_Textures) {
			std::string number;
			if (tex.Type == "texture_diffuse")       number = std::to_string(diffuseNr++);
			else if (tex.Type == "texture_specular") number = std::to_string(specularNr++);
			else if (tex.Type == "texture_normal")   number = std::to_string(normalNr++);
			else if (tex.Type == "texture_height")   number = std::to_string(heightNr++);
			m_TextureUniformNames.push_back(tex.Type + number);
		}

		m_VertexArray = VertexArray::Create();

		m_VertexBuffer = VertexBuffer::Create((float*)m_Vertices.data(), (uint32_t)(m_Vertices.size() * sizeof(Vertex)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoords" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Bitangent" },
			{ ShaderDataType::Int,    "a_EntityID" }
		};

		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);

		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size());
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
	}

	void Mesh::SetBoneData(const std::vector<glm::ivec4>& boneIDs, const std::vector<glm::vec4>& boneWeights) {
		if (boneIDs.size() != m_Vertices.size() || boneWeights.size() != m_Vertices.size()) return;

		struct BoneVertex {
			glm::ivec4 IDs;
			glm::vec4 Weights;
		};
		std::vector<BoneVertex> data(m_Vertices.size());
		for (size_t i = 0; i < data.size(); i++) {
			data[i].IDs = boneIDs[i];
			data[i].Weights = boneWeights[i];
		}

		m_BoneBuffer = VertexBuffer::Create(reinterpret_cast<float*>(data.data()),
			static_cast<uint32_t>(data.size() * sizeof(BoneVertex)));
		m_BoneBuffer->SetLayout({
			{ ShaderDataType::Int4,   "a_BoneIDs" },
			{ ShaderDataType::Float4, "a_BoneWeights" }
		});
		m_VertexArray->AddVertexBuffer(m_BoneBuffer);
		m_Skinned = true;
	}

	void Mesh::SetEntityID(int entityID) {
		for (auto& vertex : m_Vertices) {
			vertex.EntityID = entityID;
		}

		m_VertexBuffer->SetData(m_Vertices.data(),
			(uint32_t)(m_Vertices.size() * sizeof(Vertex)));
	}

	void Mesh::TransformAABB(const glm::mat4& m, const glm::vec3& localMin, const glm::vec3& localMax,
		glm::vec3& outMin, glm::vec3& outMax) {
		glm::vec3 center = (localMin + localMax) * 0.5f;
		glm::vec3 extents = (localMax - localMin) * 0.5f;
		glm::vec3 worldCenter = glm::vec3(m * glm::vec4(center, 1.0f));
		glm::vec3 worldExtents(
			std::abs(m[0][0]) * extents.x + std::abs(m[1][0]) * extents.y + std::abs(m[2][0]) * extents.z,
			std::abs(m[0][1]) * extents.x + std::abs(m[1][1]) * extents.y + std::abs(m[2][1]) * extents.z,
			std::abs(m[0][2]) * extents.x + std::abs(m[1][2]) * extents.y + std::abs(m[2][2]) * extents.z);
		outMin = worldCenter - worldExtents;
		outMax = worldCenter + worldExtents;
	}

	void Mesh::DrawGeometry() {
		m_VertexArray->Bind();
		if (auto* cmdList = RHI::GetImmediateCommandList())
			cmdList->DrawIndexed((uint32_t)m_Indices.size());
	}

	void Mesh::Draw(const Ref<Shader>& shader) {
		for (uint32_t i = 0; i < m_Textures.size(); i++) {
			if (shader) shader->SetInt(m_TextureUniformNames[i], i);
			m_Textures[i].Texture->Bind(i);
		}

		m_VertexArray->Bind();
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->DrawIndexed((uint32_t)m_Indices.size());
		}
	}

} // namespace Lunex