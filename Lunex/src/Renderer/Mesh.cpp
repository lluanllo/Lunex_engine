#include "stpch.h"
#include "Mesh.h"

#include "Renderer/RenderCommand.h"

namespace Lunex {

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<MeshTexture>& textures)
		: m_Vertices(vertices), m_Indices(indices), m_Textures(textures)
	{
		SetupMesh();
	}

	void Mesh::SetupMesh()
	{
		m_VertexArray = VertexArray::Create();

		m_VertexBuffer = VertexBuffer::Create((float*)m_Vertices.data(), (uint32_t)(m_Vertices.size() * sizeof(Vertex)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoords" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Bitangent" }
		};

		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);

		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size());
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
	}

	void Mesh::Draw(const Ref<Shader>& shader)
	{
		uint32_t diffuseNr = 1;
		uint32_t specularNr = 1;
		uint32_t normalNr = 1;
		uint32_t heightNr = 1;

		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
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

		// Draw mesh
		m_VertexArray->Bind();
		RenderCommand::DrawIndexed(m_VertexArray, (uint32_t)m_Indices.size());
	}

}
