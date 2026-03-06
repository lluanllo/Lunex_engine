#include "stpch.h"
#include "Model.h"

#include "Log/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "stb_image.h"

namespace Lunex {

	// Helper to convert aiMatrix4x4 to glm::mat4
	static glm::mat4 AiMatrix4x4ToGlm(const aiMatrix4x4& from) {
		glm::mat4 to;
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

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
			aiProcess_CalcTangentSpace |
			aiProcess_FlipUVs |
			aiProcess_JoinIdenticalVertices |
			aiProcess_OptimizeMeshes);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			LNX_LOG_ERROR("ERROR::ASSIMP::{0}", importer.GetErrorString());
			return;
		}

		m_Directory = path.substr(0, path.find_last_of('/'));

		// Also check for backslash (Windows paths)
		if (m_Directory == path) {
			m_Directory = path.substr(0, path.find_last_of('\\'));
		}

		// Pre-extract unique materials from the scene (deduplicated by Assimp material index)
		// This avoids duplicating material data when multiple meshes share the same material
		std::unordered_map<unsigned int, uint32_t> assimpMatToIndex; // Assimp material index -> m_MaterialData index
		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
			ModelMaterialData matData = ExtractMaterialData(scene->mMaterials[i], scene);
			uint32_t idx = static_cast<uint32_t>(m_MaterialData.size());
			m_MaterialData.push_back(matData);
			assimpMatToIndex[i] = idx;
		}

		// Store the mapping for use during ProcessMesh
		m_AssimpMatToIndex = assimpMatToIndex;

		ProcessNode(scene->mRootNode, scene, glm::mat4(1.0f));

		// Clear the temporary mapping
		m_AssimpMatToIndex.clear();

		LNX_LOG_INFO("Model loaded successfully: {0}, Meshes: {1}, Unique Materials: {2}", path, m_Meshes.size(), m_MaterialData.size());
	}

	void Model::ProcessNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
	{
		// Accumulate this node's transform with the parent
		glm::mat4 nodeTransform = AiMatrix4x4ToGlm(node->mTransformation);
		glm::mat4 globalTransform = parentTransform * nodeTransform;

		// Process all the node's meshes
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(ProcessMesh(mesh, scene, globalTransform));
		}

		// Process children nodes
		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, globalTransform);
		}
	}

	std::string Model::ResolveTexturePath(const std::string& texPath, const aiScene* scene)
	{
		// Handle embedded textures (GLTF stores them as "*0", "*1", etc.)
		if (!texPath.empty() && texPath[0] == '*') {
			int texIndex = std::atoi(texPath.c_str() + 1);
			if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
				const aiTexture* embeddedTex = scene->mTextures[texIndex];
				
				// If the texture is compressed (jpg/png embedded), extract to a temp file
				if (embeddedTex->mHeight == 0) {
					// Compressed format - mWidth is the buffer size in bytes
					// Generate a deterministic filename based on directory + index
					std::string extension = ".png";
					if (embeddedTex->achFormatHint[0] != '\0') {
						std::string hint(embeddedTex->achFormatHint);
						if (hint == "jpg" || hint == "jpeg") extension = ".jpg";
						else if (hint == "png") extension = ".png";
						else if (hint == "tga") extension = ".tga";
						else if (hint == "bmp") extension = ".bmp";
					}
					
					std::string extractedPath = m_Directory + "/embedded_tex_" + std::to_string(texIndex) + extension;
					
					// Only extract if the file doesn't already exist
					if (!std::filesystem::exists(extractedPath)) {
						std::ofstream outFile(extractedPath, std::ios::binary);
						if (outFile.is_open()) {
							outFile.write(reinterpret_cast<const char*>(embeddedTex->pcData), embeddedTex->mWidth);
							outFile.close();
							LNX_LOG_INFO("Extracted embedded texture [{0}] -> {1}", texIndex, extractedPath);
						}
						else {
							LNX_LOG_WARN("Failed to extract embedded texture [{0}] to {1}", texIndex, extractedPath);
							return "";
						}
					}
					return extractedPath;
				}
				else {
					// Raw ARGB8888 data - create texture from memory
					// For now, save as a file
					std::string extractedPath = m_Directory + "/embedded_tex_" + std::to_string(texIndex) + ".png";
					if (!std::filesystem::exists(extractedPath)) {
						// Would need stbi_write here, for now log warning
						LNX_LOG_WARN("Raw embedded texture [{0}] - uncompressed format not yet supported for extraction", texIndex);
						return "";
					}
					return extractedPath;
				}
			}
			return "";
		}

		// Normal file path
		std::string resolvedPath = m_Directory + "/" + texPath;
		
		// Try forward slash path
		if (std::filesystem::exists(resolvedPath)) {
			return resolvedPath;
		}
		
		// Try backslash replacement
		std::string altPath = texPath;
		std::replace(altPath.begin(), altPath.end(), '\\', '/');
		resolvedPath = m_Directory + "/" + altPath;
		if (std::filesystem::exists(resolvedPath)) {
			return resolvedPath;
		}
		
		// Try just the filename (some models reference textures by name only)
		std::filesystem::path fsPath(texPath);
		resolvedPath = m_Directory + "/" + fsPath.filename().string();
		if (std::filesystem::exists(resolvedPath)) {
			return resolvedPath;
		}

		// Return the original constructed path even if it doesn't exist
		return m_Directory + "/" + texPath;
	}

	ModelMaterialData Model::ExtractMaterialData(aiMaterial* material, const aiScene* scene)
	{
		ModelMaterialData data;

		// Get material name
		aiString matName;
		if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
			data.Name = matName.C_Str();
		}

		// ===== ALBEDO / BASE COLOR =====
		aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
			data.AlbedoColor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
		}
		else if (material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
			data.AlbedoColor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
		}

		// ===== METALLIC =====
		float metallic = 0.0f;
		if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
			data.Metallic = metallic;
		}

		// ===== ROUGHNESS =====
		float roughness = 0.5f;
		if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
			data.Roughness = roughness;
		}

		// ===== EMISSION =====
		aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
			data.EmissionColor = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
			float maxEmission = glm::max(glm::max(data.EmissionColor.r, data.EmissionColor.g), data.EmissionColor.b);
			if (maxEmission > 1.0f) {
				data.EmissionIntensity = maxEmission;
				data.EmissionColor /= maxEmission;
			}
			else if (maxEmission > 0.0f) {
				data.EmissionIntensity = 1.0f;
			}
		}

		// ===== TEXTURES =====
		aiString texPath;

		// Albedo / Base Color texture
		if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
			data.AlbedoTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}
		else if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
			data.AlbedoTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}

		// Normal map
		if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
			data.NormalTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}
		else if (material->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
			data.NormalTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}

		// Metallic-Roughness combined (GLTF PBR)
		if (material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &texPath) == AI_SUCCESS) {
			data.MetallicRoughnessTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
			data.HasMetallicRoughnessMap = true;
		}

		// Separate metallic
		if (!data.HasMetallicRoughnessMap) {
			if (material->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
				data.MetallicTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
			}
		}

		// Separate roughness
		if (!data.HasMetallicRoughnessMap) {
			if (material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS) {
				data.RoughnessTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
			}
			else if (material->GetTexture(aiTextureType_SHININESS, 0, &texPath) == AI_SUCCESS) {
				data.RoughnessTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
			}
		}

		// AO
		if (material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS) {
			data.AOTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}
		else if (material->GetTexture(aiTextureType_LIGHTMAP, 0, &texPath) == AI_SUCCESS) {
			data.AOTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}
		else if (material->GetTexture(aiTextureType_AMBIENT, 0, &texPath) == AI_SUCCESS) {
			data.AOTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}

		// Emissive
		if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
			data.EmissionTexturePath = ResolveTexturePath(texPath.C_Str(), scene);
		}

		LNX_LOG_TRACE("Extracted material '{0}': albedo=({1},{2},{3}), metallic={4}, roughness={5}",
			data.Name, data.AlbedoColor.r, data.AlbedoColor.g, data.AlbedoColor.b,
			data.Metallic, data.Roughness);

		return data;
	}

	Ref<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<MeshTexture> textures;

		bool hasUVs = mesh->HasTextureCoords(0);
		LNX_LOG_TRACE("Processing mesh: {0} vertices, Has UVs: {1}, Has Normals: {2}",
			mesh->mNumVertices,
			hasUVs ? "Yes" : "No",
			mesh->HasNormals() ? "Yes" : "No");

		// Normal matrix for transforming normals (inverse transpose of upper-left 3x3)
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

		// Process vertices
		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			// Position - apply node transform
			glm::vec4 pos = transform * glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
			vertex.Position = glm::vec3(pos);

			// Normals - apply normal matrix
			if (mesh->HasNormals())
			{
				glm::vec3 normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
				vertex.Normal = glm::normalize(normalMatrix * normal);
			}
			else
			{
				vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			}

			// Texture coordinates
			if (hasUVs)
			{
				float u = mesh->mTextureCoords[0][i].x;
				float v = mesh->mTextureCoords[0][i].y;
				vertex.TexCoords = glm::vec2(u, v);
			}
			else
			{
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			}

			// Tangent and Bitangent - apply normal matrix
			if (mesh->HasTangentsAndBitangents())
			{
				glm::vec3 tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
				glm::vec3 bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
				vertex.Tangent = glm::normalize(normalMatrix * tangent);
				vertex.Bitangent = glm::normalize(normalMatrix * bitangent);
			}
			else
			{
				vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			}

			vertices.push_back(vertex);
		}

		// Calculate tangents and bitangents if they weren't provided by Assimp
		if (!mesh->HasTangentsAndBitangents() && hasUVs)
		{
			LNX_LOG_TRACE("Calculating tangents and bitangents manually");

			for (auto& vertex : vertices)
			{
				vertex.Tangent = glm::vec3(0.0f);
				vertex.Bitangent = glm::vec3(0.0f);
			}

			for (uint32_t i = 0; i < mesh->mNumFaces; i++)
			{
				aiFace face = mesh->mFaces[i];
				if (face.mNumIndices == 3)
				{
					uint32_t i0 = face.mIndices[0];
					uint32_t i1 = face.mIndices[1];
					uint32_t i2 = face.mIndices[2];

					glm::vec3& v0 = vertices[i0].Position;
					glm::vec3& v1 = vertices[i1].Position;
					glm::vec3& v2 = vertices[i2].Position;

					glm::vec2& uv0 = vertices[i0].TexCoords;
					glm::vec2& uv1 = vertices[i1].TexCoords;
					glm::vec2& uv2 = vertices[i2].TexCoords;

					glm::vec3 edge1 = v1 - v0;
					glm::vec3 edge2 = v2 - v0;
					glm::vec2 deltaUV1 = uv1 - uv0;
					glm::vec2 deltaUV2 = uv2 - uv0;

					float denominator = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
					float f = (abs(denominator) > 0.0001f) ? 1.0f / denominator : 0.0f;

					glm::vec3 tangent;
					tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
					tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
					tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

					glm::vec3 bitangent;
					bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
					bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
					bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

					vertices[i0].Tangent += tangent;
					vertices[i1].Tangent += tangent;
					vertices[i2].Tangent += tangent;

					vertices[i0].Bitangent += bitangent;
					vertices[i1].Bitangent += bitangent;
					vertices[i2].Bitangent += bitangent;
				}
			}

			for (auto& vertex : vertices)
			{
				glm::vec3 n = vertex.Normal;
				glm::vec3 t = vertex.Tangent;

				if (glm::length(t) > 0.0001f) {
					t = glm::normalize(t - n * glm::dot(n, t));

					if (glm::dot(glm::cross(n, t), vertex.Bitangent) < 0.0f)
					{
						t = t * -1.0f;
					}

					vertex.Tangent = t;
					vertex.Bitangent = glm::cross(n, t);
				}
				else {
					vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
					vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
				}
			}
		}

		// Process indices
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		// Process material and extract material data
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			// Map this submesh to its unique material index (already extracted in LoadModel)
			auto it = m_AssimpMatToIndex.find(mesh->mMaterialIndex);
			if (it != m_AssimpMatToIndex.end()) {
				m_SubmeshMaterialIndices.push_back(it->second);
			} else {
				// Fallback: extract and add (should not happen with pre-extraction)
				ModelMaterialData matData = ExtractMaterialData(material, scene);
				uint32_t idx = static_cast<uint32_t>(m_MaterialData.size());
				m_MaterialData.push_back(matData);
				m_SubmeshMaterialIndices.push_back(idx);
			}

			// Load diffuse/albedo maps
			std::vector<MeshTexture> diffuseMaps = LoadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", scene);
			if (diffuseMaps.empty()) {
				diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
			}
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

			// Load normal maps
			std::vector<MeshTexture> normalMaps = LoadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
			if (normalMaps.empty()) {
				normalMaps = LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
			}
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

			// Load metallic maps
			std::vector<MeshTexture> metallicMaps = LoadMaterialTextures(material, aiTextureType_METALNESS, "texture_metallic", scene);
			textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());

			// Load roughness maps
			std::vector<MeshTexture> roughnessMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "texture_roughness", scene);
			if (roughnessMaps.empty()) {
				roughnessMaps = LoadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
			}
			textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

			// Load AO maps
			std::vector<MeshTexture> aoMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT_OCCLUSION, "texture_ao", scene);
			if (aoMaps.empty()) {
				aoMaps = LoadMaterialTextures(material, aiTextureType_LIGHTMAP, "texture_ao", scene);
			}
			if (aoMaps.empty()) {
				aoMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_ao", scene);
			}
			textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

			// Load emissive maps
			std::vector<MeshTexture> emissiveMaps = LoadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_emissive", scene);
			textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
		}
		else {
			// No material - point to first material or add a default
			m_SubmeshMaterialIndices.push_back(0);
		}

		LNX_LOG_TRACE("Mesh created with {0} vertices, {1} indices, {2} textures",
			vertices.size(), indices.size(), textures.size());

		return CreateRef<Mesh>(vertices, indices, textures);
	}

	std::vector<MeshTexture> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName, const aiScene* scene)
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
				std::string texturePath = ResolveTexturePath(str.C_Str(), scene);

				if (texturePath.empty()) {
					LNX_LOG_WARN("Could not resolve texture path: {0}", str.C_Str());
					continue;
				}

				MeshTexture texture;
				texture.Texture = Texture2D::Create(texturePath);
				texture.Type = typeName;
				texture.Path = str.C_Str();

				if (texture.Texture && texture.Texture->IsLoaded()) {
					textures.push_back(texture);
					m_TexturesLoaded.push_back(texture);
					LNX_LOG_TRACE("Loaded texture [{0}]: {1}", typeName, texturePath);
				}
				else {
					LNX_LOG_WARN("Failed to load texture: {0}", texturePath);
				}
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

		// Front face
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });

		// Back face
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} });

		// Top face
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });

		// Bottom face
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });

		// Right face
		vertices.push_back({ { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f} });

		// Left face
		vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });
		vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} });

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

				// Calculate tangent and bitangent for sphere
				float const theta = 2 * glm::pi<float>() * s * S;
				vertex.Tangent = glm::normalize(glm::vec3(-sin(theta), 0.0f, cos(theta)));
				vertex.Bitangent = glm::normalize(glm::cross(vertex.Normal, vertex.Tangent));

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

		// Plane vertices (Y-up) with proper tangents
		vertices.push_back({ {-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ { 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });
		vertices.push_back({ {-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} });

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

			glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
			glm::vec3 tangent = glm::normalize(glm::vec3(-sin(angle), 0.0f, cos(angle)));
			glm::vec3 bitangent = glm::vec3(0.0f, 1.0f, 0.0f);

			// Top vertex
			vertices.push_back({ {x, halfHeight, z}, normal, {(float)i / segments, 1.0f}, tangent, bitangent });
			// Bottom vertex
			vertices.push_back({ {x, -halfHeight, z}, normal, {(float)i / segments, 0.0f}, tangent, bitangent });
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

	void Model::SetEntityID(int entityID) {
		for (auto& mesh : m_Meshes) {
			mesh->SetEntityID(entityID);
		}
	}
}
