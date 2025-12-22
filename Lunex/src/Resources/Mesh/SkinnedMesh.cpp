#include "stpch.h"
#include "SkinnedMesh.h"

#include "RHI/RHI.h"

namespace Lunex {

	SkinnedMesh::SkinnedMesh(
		const std::vector<SkinnedVertex>& vertices,
		const std::vector<uint32_t>& indices,
		const std::vector<MeshTexture>& textures)
		: m_Vertices(vertices)
		, m_Indices(indices)
		, m_Textures(textures)
	{
		// Check if any vertex has bone weights
		m_HasBoneWeights = false;
		for (const auto& v : vertices) {
			float totalWeight = v.BoneWeights.x + v.BoneWeights.y + v.BoneWeights.z + v.BoneWeights.w;
			if (totalWeight > 0.0f) {
				m_HasBoneWeights = true;
				break;
			}
		}
		
		SetupMesh();
	}
	
	void SkinnedMesh::SetupMesh() {
		m_VertexArray = VertexArray::Create();
		
		m_VertexBuffer = VertexBuffer::Create(
			(float*)m_Vertices.data(),
			(uint32_t)(m_Vertices.size() * sizeof(SkinnedVertex))
		);
		
		// Extended layout with bone data
		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoords" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Bitangent" },
			{ ShaderDataType::Int,    "a_EntityID" },
			{ ShaderDataType::Int4,   "a_BoneIDs" },      // ? Bone indices
			{ ShaderDataType::Float4, "a_BoneWeights" }   // ? Bone weights
		};
		
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);
		
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size());
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
	}
	
	void SkinnedMesh::SetEntityID(int entityID) {
		for (auto& vertex : m_Vertices) {
			vertex.EntityID = entityID;
		}
		
		m_VertexBuffer->SetData(
			m_Vertices.data(),
			(uint32_t)(m_Vertices.size() * sizeof(SkinnedVertex))
		);
	}
	
	void SkinnedMesh::Draw(const Ref<Shader>& shader) {
		// Bind textures
		uint32_t diffuseNr = 1;
		uint32_t specularNr = 1;
		uint32_t normalNr = 1;
		uint32_t heightNr = 1;
		
		for (uint32_t i = 0; i < m_Textures.size(); i++) {
			std::string number;
			std::string name = m_Textures[i].Type;
			
			if (name == "texture_diffuse")
				number = std::to_string(diffuseNr++);
			else if (name == "texture_specular")
				number = std::to_string(specularNr++);
			else if (name == "texture_normal")
				number = std::to_string(normalNr++);
			else if (name == "texture_height")
				number = std::to_string(heightNr++);
			
			shader->SetInt((name + number).c_str(), i);
			m_Textures[i].Texture->Bind(i);
		}
		
		// Draw
		m_VertexArray->Bind();
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->DrawIndexed((uint32_t)m_Indices.size());
		}
	}

}
