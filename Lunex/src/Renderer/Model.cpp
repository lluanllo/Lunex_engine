#include "stpch.h"
#include "Model.h"

#include "Log/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Lunex {

	Model::Model(const std::string& path)
	{
		LoadModel(path);
	}

	void Model::LoadModel(const std::string& path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, 
			aiProcess_Triangulate | 
			aiProcess_GenSmoothNormals | 
			aiProcess_FlipUVs | 
			aiProcess_CalcTangentSpace);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			LNX_LOG_ERROR("ERROR::ASSIMP::{0}", importer.GetErrorString());
			return;
		}

		m_Directory = path.substr(0, path.find_last_of('/'));

		ProcessNode(scene->mRootNode, scene);

		LNX_LOG_INFO("Model loaded successfully: {0}, Meshes: {1}", path, m_Meshes.size());
	}

	void Model::ProcessNode(aiNode* node, const aiScene* scene)
	{
		// Process all the node's meshes
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(ProcessMesh(mesh, scene));
		}

		// Process children nodes
		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene);
		}
	}

	Ref<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<MeshTexture> textures;

		// Process vertices
		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			// Position
			vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

			// Normals
			if (mesh->HasNormals())
				vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			else
				vertex.Normal = glm::vec3(0.0f);

			// Texture coordinates
			if (mesh->mTextureCoords[0])
			{
				vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
			}
			else
				vertex.TexCoords = glm::vec2(0.0f);

			// Tangent
			if (mesh->HasTangentsAndBitangents())
			{
				vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
				vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
			}
			else
			{
				vertex.Tangent = glm::vec3(0.0f);
				vertex.Bitangent = glm::vec3(0.0f);
			}

			vertices.push_back(vertex);
		}

		// Process indices
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		// Process material
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			std::vector<MeshTexture> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

			std::vector<MeshTexture> specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

			std::vector<MeshTexture> normalMaps = LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

			std::vector<MeshTexture> heightMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
			textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
		}

		return CreateRef<Mesh>(vertices, indices, textures);
	}

	std::vector<MeshTexture> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName)
	{
		std::vector<MeshTexture> textures;

		for (uint32_t i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);

			bool skip = false;
			for (uint32_t j = 0; j < m_TexturesLoaded.size(); j++)
			{
				if (std::strcmp(m_TexturesLoaded[j].Path.data(), str.C_Str()) == 0)
				{
					textures.push_back(m_TexturesLoaded[j]);
					skip = true;
					break;
				}
			}

			if (!skip)
			{
				MeshTexture texture;
				std::string texturePath = m_Directory + "/" + std::string(str.C_Str());
				texture.Texture = Texture2D::Create(texturePath);
				texture.Type = typeName;
				texture.Path = str.C_Str();

				textures.push_back(texture);
				m_TexturesLoaded.push_back(texture);
			}
		}

		return textures;
	}

	void Model::Draw(const Ref<Shader>& shader)
	{
		for (auto& mesh : m_Meshes)
			mesh->Draw(shader);
	}

	// ============================================================================
	// PRIMITIVE GENERATORS
	// ============================================================================

	Ref<Model> Model::CreateCube()
	{
		Ref<Model> model = CreateRef<Model>();

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// Cube vertices with normals
		// Front face
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} });

		// Back face
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f} });

		// Top face
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} });

		// Bottom face
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f} });

		// Right face
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} });

		// Left face
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} });

		// Indices
		for (uint32_t i = 0; i < 6; i++)
		{
			uint32_t offset = i * 4;
			indices.push_back(offset + 0);
			indices.push_back(offset + 1);
			indices.push_back(offset + 2);
			indices.push_back(offset + 2);
			indices.push_back(offset + 3);
			indices.push_back(offset + 0);
		}

		model->m_Meshes.push_back(CreateRef<Mesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

	Ref<Model> Model::CreateSphere(uint32_t segments)
	{
		Ref<Model> model = CreateRef<Model>();

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		const float radius = 0.5f;
		const uint32_t rings = segments;
		const uint32_t sectors = segments;

		float const R = 1.0f / (float)(rings - 1);
		float const S = 1.0f / (float)(sectors - 1);

		for (uint32_t r = 0; r < rings; r++)
		{
			for (uint32_t s = 0; s < sectors; s++)
			{
				float const y = sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
				float const x = cos(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);
				float const z = sin(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);

				Vertex vertex;
				vertex.Position = glm::vec3(x * radius, y * radius, z * radius);
				vertex.Normal = glm::normalize(glm::vec3(x, y, z));
				vertex.TexCoords = glm::vec2(s * S, r * R);
				vertex.Tangent = glm::vec3(0.0f);
				vertex.Bitangent = glm::vec3(0.0f);

				vertices.push_back(vertex);
			}
		}

		for (uint32_t r = 0; r < rings - 1; r++)
		{
			for (uint32_t s = 0; s < sectors - 1; s++)
			{
				indices.push_back(r * sectors + s);
				indices.push_back(r * sectors + (s + 1));
				indices.push_back((r + 1) * sectors + (s + 1));

				indices.push_back(r * sectors + s);
				indices.push_back((r + 1) * sectors + (s + 1));
				indices.push_back((r + 1) * sectors + s);
			}
		}

		model->m_Meshes.push_back(CreateRef<Mesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

	Ref<Model> Model::CreatePlane()
	{
		Ref<Model> model = CreateRef<Model>();

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// Plane vertices (Y-up)
		vertices.push_back({ {-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} });
		vertices.push_back({ { 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} });
		vertices.push_back({ {-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} });

		// Indices
		indices = { 0, 1, 2, 2, 3, 0 };

		model->m_Meshes.push_back(CreateRef<Mesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

	Ref<Model> Model::CreateCylinder(uint32_t segments)
	{
		Ref<Model> model = CreateRef<Model>();

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		const float radius = 0.5f;
		const float height = 1.0f;
		const float halfHeight = height * 0.5f;

		// Top and bottom circles
		for (uint32_t i = 0; i <= segments; i++)
		{
			float angle = (float)i / (float)segments * glm::two_pi<float>();
			float x = cos(angle) * radius;
			float z = sin(angle) * radius;

			// Top vertex
			vertices.push_back({ {x, halfHeight, z}, {x / radius, 0.0f, z / radius}, {(float)i / segments, 1.0f} });
			// Bottom vertex
			vertices.push_back({ {x, -halfHeight, z}, {x / radius, 0.0f, z / radius}, {(float)i / segments, 0.0f} });
		}

		// Side faces
		for (uint32_t i = 0; i < segments; i++)
		{
			uint32_t topCurrent = i * 2;
			uint32_t bottomCurrent = i * 2 + 1;
			uint32_t topNext = (i + 1) * 2;
			uint32_t bottomNext = (i + 1) * 2 + 1;

			indices.push_back(topCurrent);
			indices.push_back(bottomCurrent);
			indices.push_back(topNext);

			indices.push_back(topNext);
			indices.push_back(bottomCurrent);
			indices.push_back(bottomNext);
		}

		model->m_Meshes.push_back(CreateRef<Mesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

}
