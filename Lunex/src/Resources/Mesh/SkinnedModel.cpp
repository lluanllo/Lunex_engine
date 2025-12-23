#include "stpch.h"
#include "SkinnedModel.h"

#include "Log/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Lunex {

	// ============================================================================
	// HELPER: Convert Assimp matrix to GLM
	// ============================================================================
	static glm::mat4 AssimpToGlmMatrix(const aiMatrix4x4& m) {
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	}

	// ============================================================================
	// CONSTRUCTOR
	// ============================================================================

	SkinnedModel::SkinnedModel(const std::string& path) {
		LoadModel(path);
	}

	// ============================================================================
	// LOAD MODEL WITH BONE WEIGHTS
	// ============================================================================

	void SkinnedModel::LoadModel(const std::string& path) {
		Assimp::Importer importer;

		// Import flags - include bone data
		unsigned int flags = 
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_LimitBoneWeights |     // Limit to 4 bones per vertex
			aiProcess_JoinIdenticalVertices;

		const aiScene* scene = importer.ReadFile(path, flags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			LNX_LOG_ERROR("SkinnedModel::LoadModel - Assimp error: {0}", importer.GetErrorString());
			return;
		}

		// Extract directory
		m_Directory = path.substr(0, path.find_last_of('/'));
		if (m_Directory == path) {
			m_Directory = path.substr(0, path.find_last_of('\\'));
		}

		// Check if model has bones
		bool hasBones = false;
		for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
			if (scene->mMeshes[i]->mNumBones > 0) {
				hasBones = true;
				break;
			}
		}

		if (hasBones) {
			LNX_LOG_INFO("SkinnedModel: Loading skeletal mesh from {0}", path);
		} else {
			LNX_LOG_INFO("SkinnedModel: Loading static mesh from {0} (no bones found)", path);
		}

		// Process nodes recursively
		ProcessNode(scene->mRootNode, scene);

		LNX_LOG_INFO("SkinnedModel: Loaded {0} meshes, {1} bones from {2}", 
			m_Meshes.size(), m_BoneCounter, path);
	}

	// ============================================================================
	// PROCESS NODE
	// ============================================================================

	void SkinnedModel::ProcessNode(aiNode* node, const aiScene* scene) {
		// Process all meshes in this node
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(ProcessMesh(mesh, scene));
		}

		// Process children
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			ProcessNode(node->mChildren[i], scene);
		}
	}

	// ============================================================================
	// PROCESS MESH WITH BONE WEIGHTS
	// ============================================================================

	Ref<SkinnedMesh> SkinnedModel::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
		std::vector<SkinnedVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<MeshTexture> textures;

		bool hasUVs = mesh->HasTextureCoords(0);
		
		LNX_LOG_TRACE("SkinnedModel: Processing mesh with {0} vertices, {1} bones",
			mesh->mNumVertices, mesh->mNumBones);

		// ========================================
		// STEP 1: Process vertices (position, normal, UV, tangent)
		// ========================================
		vertices.reserve(mesh->mNumVertices);
		
		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
			SkinnedVertex vertex;
			
			// Initialize bone data to default
			SetVertexBoneDataToDefault(vertex);

			// Position
			vertex.Position = glm::vec3(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z
			);

			// Normal
			if (mesh->HasNormals()) {
				vertex.Normal = glm::vec3(
					mesh->mNormals[i].x,
					mesh->mNormals[i].y,
					mesh->mNormals[i].z
				);
			} else {
				vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			}

			// Texture coordinates
			if (hasUVs) {
				vertex.TexCoords = glm::vec2(
					mesh->mTextureCoords[0][i].x,
					mesh->mTextureCoords[0][i].y
				);
			} else {
				vertex.TexCoords = glm::vec2(0.0f);
			}

			// Tangent and Bitangent
			if (mesh->HasTangentsAndBitangents()) {
				vertex.Tangent = glm::vec3(
					mesh->mTangents[i].x,
					mesh->mTangents[i].y,
					mesh->mTangents[i].z
				);
				vertex.Bitangent = glm::vec3(
					mesh->mBitangents[i].x,
					mesh->mBitangents[i].y,
					mesh->mBitangents[i].z
				);
			} else {
				vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			}

			vertices.push_back(vertex);
		}

		// ========================================
		// STEP 2: Extract bone weights
		// ========================================
		ExtractBoneWeights(vertices, mesh, scene);

		// ========================================
		// STEP 3: Process indices
		// ========================================
		for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++) {
				indices.push_back(face.mIndices[j]);
			}
		}

		// ========================================
		// STEP 4: Load textures from material
		// ========================================
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			auto diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

			auto specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

			auto normalMaps = LoadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
			if (normalMaps.empty()) {
				normalMaps = LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
			}
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
		}

		LNX_LOG_TRACE("SkinnedModel: Created mesh with {0} vertices, {1} indices, {2} textures",
			vertices.size(), indices.size(), textures.size());

		return CreateRef<SkinnedMesh>(vertices, indices, textures);
	}

	// ============================================================================
	// EXTRACT BONE WEIGHTS FROM ASSIMP
	// ============================================================================

	void SkinnedModel::ExtractBoneWeights(
		std::vector<SkinnedVertex>& vertices,
		aiMesh* mesh,
		const aiScene* scene)
	{
		// Iterate through all bones in this mesh
		for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string boneName = bone->mName.C_Str();
			
			int boneID = -1;

			// Check if bone already exists in our map
			auto it = m_BoneInfoMap.find(boneName);
			if (it == m_BoneInfoMap.end()) {
				// New bone - add to map
				BoneInfo newBone;
				newBone.ID = m_BoneCounter;
				newBone.OffsetMatrix = AssimpToGlmMatrix(bone->mOffsetMatrix);
				
				m_BoneInfoMap[boneName] = newBone;
				boneID = m_BoneCounter;
				m_BoneCounter++;
				
				LNX_LOG_TRACE("SkinnedModel: Added bone '{0}' with ID {1}", boneName, boneID);
			} else {
				// Existing bone
				boneID = it->second.ID;
			}

			// Apply bone weights to affected vertices
			for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
				aiVertexWeight& weight = bone->mWeights[weightIndex];
				
				unsigned int vertexID = weight.mVertexId;
				float boneWeight = weight.mWeight;

				if (vertexID >= vertices.size()) {
					LNX_LOG_WARN("SkinnedModel: Invalid vertex ID {0} for bone '{1}'", vertexID, boneName);
					continue;
				}

				// Add bone influence to vertex
				vertices[vertexID].AddBoneInfluence(boneID, boneWeight);
			}
		}

		// Normalize bone weights for all vertices
		for (auto& vertex : vertices) {
			vertex.NormalizeBoneWeights();
		}

		LNX_LOG_TRACE("SkinnedModel: Extracted weights from {0} bones", mesh->mNumBones);
	}

	// ============================================================================
	// SET DEFAULT BONE DATA
	// ============================================================================

	void SkinnedModel::SetVertexBoneDataToDefault(SkinnedVertex& vertex) {
		vertex.BoneIDs = glm::ivec4(-1);
		vertex.BoneWeights = glm::vec4(0.0f);
	}

	// ============================================================================
	// LOAD MATERIAL TEXTURES
	// ============================================================================

	std::vector<MeshTexture> SkinnedModel::LoadMaterialTextures(
		aiMaterial* mat,
		aiTextureType type,
		const std::string& typeName)
	{
		std::vector<MeshTexture> textures;

		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);

			// Check if texture is already loaded
			bool skip = false;
			for (const auto& loaded : m_TexturesLoaded) {
				if (loaded.Path == str.C_Str()) {
					textures.push_back(loaded);
					skip = true;
					break;
				}
			}

			if (!skip) {
				MeshTexture texture;
				std::string texturePath = m_Directory + "/" + std::string(str.C_Str());
				texture.Texture = Texture2D::Create(texturePath);
				texture.Type = typeName;
				texture.Path = str.C_Str();

				if (texture.Texture && texture.Texture->IsLoaded()) {
					textures.push_back(texture);
					m_TexturesLoaded.push_back(texture);
				} else {
					LNX_LOG_WARN("SkinnedModel: Failed to load texture: {0}", texturePath);
				}
			}
		}

		return textures;
	}

	// ============================================================================
	// DRAW
	// ============================================================================

	void SkinnedModel::Draw(const Ref<Shader>& shader) {
		for (auto& mesh : m_Meshes) {
			mesh->Draw(shader);
		}
	}

	// ============================================================================
	// BONE ACCESS
	// ============================================================================

	std::vector<glm::mat4> SkinnedModel::GetInverseBindPoseMatrices() const {
		std::vector<glm::mat4> matrices(m_BoneCounter, glm::mat4(1.0f));
		
		for (const auto& [name, info] : m_BoneInfoMap) {
			if (info.ID >= 0 && info.ID < m_BoneCounter) {
				matrices[info.ID] = info.OffsetMatrix;
			}
		}
		
		return matrices;
	}

	int SkinnedModel::GetBoneIndex(const std::string& boneName) const {
		auto it = m_BoneInfoMap.find(boneName);
		if (it != m_BoneInfoMap.end()) {
			return it->second.ID;
		}
		return -1;
	}

	void SkinnedModel::SetEntityID(int entityID) {
		for (auto& mesh : m_Meshes) {
			mesh->SetEntityID(entityID);
		}
	}

	// ============================================================================
	// PRIMITIVES (No bones - for fallback)
	// ============================================================================

	Ref<SkinnedModel> SkinnedModel::CreateCube() {
		Ref<SkinnedModel> model = CreateRef<SkinnedModel>();

		std::vector<SkinnedVertex> vertices;
		std::vector<uint32_t> indices;

		// Front face
		auto addFace = [&](glm::vec3 normal, glm::vec3 tangent, glm::vec3 bitangent,
						   glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
			uint32_t offset = static_cast<uint32_t>(vertices.size());
			
			SkinnedVertex v0, v1, v2, v3;
			v0.Position = p0; v0.Normal = normal; v0.TexCoords = {0, 0}; v0.Tangent = tangent; v0.Bitangent = bitangent;
			v1.Position = p1; v1.Normal = normal; v1.TexCoords = {1, 0}; v1.Tangent = tangent; v1.Bitangent = bitangent;
			v2.Position = p2; v2.Normal = normal; v2.TexCoords = {1, 1}; v2.Tangent = tangent; v2.Bitangent = bitangent;
			v3.Position = p3; v3.Normal = normal; v3.TexCoords = {0, 1}; v3.Tangent = tangent; v3.Bitangent = bitangent;
			
			vertices.push_back(v0);
			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);
			
			indices.push_back(offset + 0);
			indices.push_back(offset + 1);
			indices.push_back(offset + 2);
			indices.push_back(offset + 2);
			indices.push_back(offset + 3);
			indices.push_back(offset + 0);
		};

		// Front
		addFace({0, 0, 1}, {1, 0, 0}, {0, 1, 0},
				{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f});
		// Back
		addFace({0, 0, -1}, {-1, 0, 0}, {0, 1, 0},
				{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f});
		// Top
		addFace({0, 1, 0}, {1, 0, 0}, {0, 0, -1},
				{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f});
		// Bottom
		addFace({0, -1, 0}, {1, 0, 0}, {0, 0, 1},
				{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f});
		// Right
		addFace({1, 0, 0}, {0, 0, -1}, {0, 1, 0},
				{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
		// Left
		addFace({-1, 0, 0}, {0, 0, 1}, {0, 1, 0},
				{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f});

		model->m_Meshes.push_back(CreateRef<SkinnedMesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

	Ref<SkinnedModel> SkinnedModel::CreateSphere(uint32_t segments) {
		Ref<SkinnedModel> model = CreateRef<SkinnedModel>();

		std::vector<SkinnedVertex> vertices;
		std::vector<uint32_t> indices;

		const float radius = 0.5f;
		const uint32_t rings = segments;
		const uint32_t sectors = segments;

		float const R = 1.0f / (float)(rings - 1);
		float const S = 1.0f / (float)(sectors - 1);

		for (uint32_t r = 0; r < rings; r++) {
			for (uint32_t s = 0; s < sectors; s++) {
				float const y = sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
				float const x = cos(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);
				float const z = sin(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);

				SkinnedVertex vertex;
				vertex.Position = glm::vec3(x * radius, y * radius, z * radius);
				vertex.Normal = glm::normalize(glm::vec3(x, y, z));
				vertex.TexCoords = glm::vec2(s * S, r * R);

				float const theta = 2 * glm::pi<float>() * s * S;
				vertex.Tangent = glm::normalize(glm::vec3(-sin(theta), 0.0f, cos(theta)));
				vertex.Bitangent = glm::normalize(glm::cross(vertex.Normal, vertex.Tangent));

				vertices.push_back(vertex);
			}
		}

		for (uint32_t r = 0; r < rings - 1; r++) {
			for (uint32_t s = 0; s < sectors - 1; s++) {
				indices.push_back(r * sectors + s);
				indices.push_back(r * sectors + (s + 1));
				indices.push_back((r + 1) * sectors + (s + 1));

				indices.push_back(r * sectors + s);
				indices.push_back((r + 1) * sectors + (s + 1));
				indices.push_back((r + 1) * sectors + s);
			}
		}

		model->m_Meshes.push_back(CreateRef<SkinnedMesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

	Ref<SkinnedModel> SkinnedModel::CreatePlane() {
		Ref<SkinnedModel> model = CreateRef<SkinnedModel>();

		std::vector<SkinnedVertex> vertices;
		std::vector<uint32_t> indices;

		SkinnedVertex v0, v1, v2, v3;
		v0.Position = {-0.5f, 0, 0.5f}; v0.Normal = {0, 1, 0}; v0.TexCoords = {0, 0}; v0.Tangent = {1, 0, 0}; v0.Bitangent = {0, 0, -1};
		v1.Position = {0.5f, 0, 0.5f};  v1.Normal = {0, 1, 0}; v1.TexCoords = {1, 0}; v1.Tangent = {1, 0, 0}; v1.Bitangent = {0, 0, -1};
		v2.Position = {0.5f, 0, -0.5f}; v2.Normal = {0, 1, 0}; v2.TexCoords = {1, 1}; v2.Tangent = {1, 0, 0}; v2.Bitangent = {0, 0, -1};
		v3.Position = {-0.5f, 0, -0.5f}; v3.Normal = {0, 1, 0}; v3.TexCoords = {0, 1}; v3.Tangent = {1, 0, 0}; v3.Bitangent = {0, 0, -1};

		vertices = { v0, v1, v2, v3 };
		indices = { 0, 1, 2, 2, 3, 0 };

		model->m_Meshes.push_back(CreateRef<SkinnedMesh>(vertices, indices, std::vector<MeshTexture>()));
		return model;
	}

}
