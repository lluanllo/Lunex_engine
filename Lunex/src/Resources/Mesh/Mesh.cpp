#include "stpch.h"
#include "Mesh.h"

#include "RHI/RHI.h"

namespace Lunex {

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
		const std::vector<MeshTexture>& textures, const MeshMaterialData& materialData)
		: m_Vertices(vertices), m_Indices(indices), m_Textures(textures), m_MaterialData(materialData),
		  m_IndexCount(static_cast<uint32_t>(indices.size()))
	{
		SetupMesh();
		CacheTextureFlags();
	}

	void Mesh::SetupMesh() {
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

		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), m_IndexCount);
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
	}

	void Mesh::Draw(const Ref<Shader>& shader) {
		// Bind per-mesh textures to the correct PBR slots expected by the shader:
		//   slot 0 = albedo/diffuse
		//   slot 1 = normal
		//   slot 2 = metallic
		//   slot 3 = roughness
		//   slot 4 = specular
		//   slot 5 = emissive
		//   slot 6 = AO
		for (uint32_t i = 0; i < m_Textures.size(); i++) {
			if (!m_Textures[i].Texture || !m_Textures[i].Texture->IsLoaded())
				continue;

			const std::string& type = m_Textures[i].Type;

			if (type == "texture_diffuse")
				m_Textures[i].Texture->Bind(0);
			else if (type == "texture_normal")
				m_Textures[i].Texture->Bind(1);
			else if (type == "texture_metallic")
				m_Textures[i].Texture->Bind(2);
			else if (type == "texture_roughness")
				m_Textures[i].Texture->Bind(3);
			else if (type == "texture_specular")
				m_Textures[i].Texture->Bind(4);
			else if (type == "texture_emissive")
				m_Textures[i].Texture->Bind(5);
			else if (type == "texture_ao")
				m_Textures[i].Texture->Bind(6);
		}

		m_VertexArray->Bind();
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->DrawIndexed(m_IndexCount);
		}
	}

	void Mesh::DrawGeometry(const Ref<Shader>& shader) {
		m_VertexArray->Bind();
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->DrawIndexed(m_IndexCount);
		}
	}

	bool Mesh::HasMeshTexture(const std::string& type) const {
		if (type == "texture_diffuse")   return (m_TextureFlags & MeshTexFlag_Diffuse) != 0;
		if (type == "texture_normal")    return (m_TextureFlags & MeshTexFlag_Normal) != 0;
		if (type == "texture_metallic")  return (m_TextureFlags & MeshTexFlag_Metallic) != 0;
		if (type == "texture_roughness") return (m_TextureFlags & MeshTexFlag_Roughness) != 0;
		if (type == "texture_specular")  return (m_TextureFlags & MeshTexFlag_Specular) != 0;
		if (type == "texture_emissive")  return (m_TextureFlags & MeshTexFlag_Emissive) != 0;
		if (type == "texture_ao")        return (m_TextureFlags & MeshTexFlag_AO) != 0;
		return false;
	}

	void Mesh::CacheTextureFlags() {
		m_TextureFlags = MeshTexFlag_None;
		for (const auto& tex : m_Textures) {
			if (!tex.Texture || !tex.Texture->IsLoaded())
				continue;

			if (tex.Type == "texture_diffuse")        m_TextureFlags |= MeshTexFlag_Diffuse;
			else if (tex.Type == "texture_normal")     m_TextureFlags |= MeshTexFlag_Normal;
			else if (tex.Type == "texture_metallic")   m_TextureFlags |= MeshTexFlag_Metallic;
			else if (tex.Type == "texture_roughness")  m_TextureFlags |= MeshTexFlag_Roughness;
			else if (tex.Type == "texture_specular")   m_TextureFlags |= MeshTexFlag_Specular;
			else if (tex.Type == "texture_emissive")   m_TextureFlags |= MeshTexFlag_Emissive;
			else if (tex.Type == "texture_ao")         m_TextureFlags |= MeshTexFlag_AO;
		}
	}

} // namespace Lunex