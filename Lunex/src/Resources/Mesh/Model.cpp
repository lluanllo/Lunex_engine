#include "stpch.h"
#include "Model.h"

#include "Log/Log.h"
#include "Core/JobSystem/JobSystem.h"
#include "Renderer/TextureCompression.h"

#include <unordered_set>
#include <functional>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

		// Removed aiProcess_FlipUVs - this can cause issues with some formats
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_LimitBoneWeights);   // at most 4 bone influences per vertex

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

		PreloadTexturesParallel(scene);
		BuildSkeleton(scene);              // before meshes: they map bone names -> indices
		ProcessNode(scene->mRootNode, scene);
		LoadAnimations(scene);
		m_PreloadedTextures.clear();

		if (IsSkinned()) {
			LNX_LOG_INFO("Model is skinned: {0} bones, {1} animation(s)", m_Skeleton->GetBoneCount(), m_Animations.size());
		}

		LNX_LOG_INFO("Model loaded successfully: {0}, Meshes: {1}", path, m_Meshes.size());
	}

	void Model::PreloadTexturesParallel(const aiScene* scene)
	{
		// Decoding (PNG/JPG/TGA -> pixels) is the slow part of loading big models like Sponza.
		// Decode every unique texture on the job workers, then upload to the GPU here on
		// the main thread (GL is not thread-safe).
		if (!JobSystem::IsInitialized())
			return;

		// The KTX compression path has its own caching and is not known to be thread-safe
		auto& compressionConfig = TextureCompressionConfig::Get();
		if (compressionConfig.EnableAutoCompression && TextureCompressionConfig::IsKTXAvailable())
			return;

		std::vector<std::string> keys;
		std::unordered_set<std::string> seen;
		for (uint32_t m = 0; m < scene->mNumMaterials; m++) {
			aiMaterial* mat = scene->mMaterials[m];
			for (int t = aiTextureType_NONE + 1; t <= aiTextureType_UNKNOWN; t++) {
				aiTextureType type = static_cast<aiTextureType>(t);
				for (uint32_t i = 0; i < mat->GetTextureCount(type); i++) {
					aiString str;
					mat->GetTexture(type, i, &str);
					std::string key = str.C_Str();
					if (key.empty() || key[0] == '*') continue; // embedded textures not supported here
					if (seen.insert(key).second) keys.push_back(key);
				}
			}
		}

		if (keys.size() < 2)
			return;

		std::vector<Texture2D::DecodedImage> decoded(keys.size());
		const std::string directory = m_Directory;

		auto counter = JobSystem::Get().ParallelFor(0, static_cast<uint32_t>(keys.size()),
			[&](uint32_t index) {
				decoded[index] = Texture2D::DecodeFromFile(directory + "/" + keys[index]);
			}, 1);
		JobSystem::Get().Wait(counter);

		for (size_t i = 0; i < keys.size(); i++) {
			if (decoded[i].IsValid()) {
				m_PreloadedTextures[keys[i]] = Texture2D::CreateFromDecoded(decoded[i]);
			}
		}

		LNX_LOG_INFO("Model: decoded {0} textures in parallel", m_PreloadedTextures.size());
	}

	static glm::mat4 ToGlm(const aiMatrix4x4& m) {
		// assimp is row-major, glm is column-major
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4);
	}

	void Model::BuildSkeleton(const aiScene* scene)
	{
		bool hasBones = false;
		for (uint32_t i = 0; i < scene->mNumMeshes && !hasBones; i++)
			hasBones = scene->mMeshes[i]->HasBones();
		if (!hasBones) return;

		m_Skeleton = CreateRef<Skeleton>();

		// Bone palette: every bone referenced by any mesh
		for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
			const aiMesh* mesh = scene->mMeshes[i];
			for (uint32_t b = 0; b < mesh->mNumBones; b++) {
				const aiBone* bone = mesh->mBones[b];
				m_Skeleton->AddBone(bone->mName.C_Str(), ToGlm(bone->mOffsetMatrix));
			}
		}

		// Node hierarchy, parents first
		std::function<void(const aiNode*, int)> addNode = [&](const aiNode* node, int parent) {
			SkeletonNode sn;
			sn.Name = node->mName.C_Str();
			sn.Parent = parent;
			sn.LocalBindTransform = ToGlm(node->mTransformation);
			auto bone = m_Skeleton->BoneIndexByName.find(sn.Name);
			sn.BoneIndex = (bone != m_Skeleton->BoneIndexByName.end()) ? bone->second : -1;

			int index = static_cast<int>(m_Skeleton->Nodes.size());
			m_Skeleton->NodeIndexByName[sn.Name] = index;
			m_Skeleton->Nodes.push_back(sn);

			for (uint32_t c = 0; c < node->mNumChildren; c++)
				addNode(node->mChildren[c], index);
		};
		addNode(scene->mRootNode, -1);

		m_Skeleton->GlobalInverse = glm::inverse(ToGlm(scene->mRootNode->mTransformation));

		if (m_Skeleton->GetBoneCount() > MAX_BONES) {
			LNX_LOG_WARN("Model has {0} bones (max {1}); extra bones will be ignored",
				m_Skeleton->GetBoneCount(), MAX_BONES);
		}
	}

	void Model::LoadAnimations(const aiScene* scene)
	{
		if (!m_Skeleton) return;

		for (uint32_t a = 0; a < scene->mNumAnimations; a++) {
			const aiAnimation* anim = scene->mAnimations[a];
			auto clip = CreateRef<AnimationClip>();
			clip->Name = anim->mName.length > 0 ? anim->mName.C_Str() : ("Animation " + std::to_string(a));
			clip->DurationTicks = static_cast<float>(anim->mDuration);
			clip->TicksPerSecond = anim->mTicksPerSecond > 0.0 ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;

			for (uint32_t c = 0; c < anim->mNumChannels; c++) {
				const aiNodeAnim* src = anim->mChannels[c];
				auto node = m_Skeleton->NodeIndexByName.find(src->mNodeName.C_Str());
				if (node == m_Skeleton->NodeIndexByName.end()) continue;

				AnimationChannel ch;
				ch.NodeIndex = node->second;
				for (uint32_t k = 0; k < src->mNumPositionKeys; k++) {
					const auto& key = src->mPositionKeys[k];
					ch.Positions.push_back({ static_cast<float>(key.mTime), glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
				}
				for (uint32_t k = 0; k < src->mNumRotationKeys; k++) {
					const auto& key = src->mRotationKeys[k];
					ch.Rotations.push_back({ static_cast<float>(key.mTime), glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z) });
				}
				for (uint32_t k = 0; k < src->mNumScalingKeys; k++) {
					const auto& key = src->mScalingKeys[k];
					ch.Scales.push_back({ static_cast<float>(key.mTime), glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
				}
				clip->Channels.push_back(std::move(ch));
			}

			m_Animations.push_back(clip);
		}
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

		bool hasUVs = mesh->HasTextureCoords(0);
		LNX_LOG_TRACE("Processing mesh: {0} vertices, Has UVs: {1}, Has Normals: {2}",
			mesh->mNumVertices,
			hasUVs ? "Yes" : "No",
			mesh->HasNormals() ? "Yes" : "No");

		// Process vertices
		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			// Position
			vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

			// Normals
			if (mesh->HasNormals())
			{
				vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			}
			else
			{
				vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			}

			// Texture coordinates - Using your working approach
			if (hasUVs)
			{
				float u = mesh->mTextureCoords[0][i].x;
				float v = mesh->mTextureCoords[0][i].y;
				vertex.TexCoords = glm::vec2(u, v);

				// Debug: Log first few UVs to verify
				if (i < 3) {
					LNX_LOG_TRACE("Vertex {0} UV: ({1}, {2})", i, u, v);
				}
			}
			else
			{
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			}

			// Tangent and Bitangent
			if (mesh->HasTangentsAndBitangents())
			{
				vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
				vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
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

			// Initialize tangent accumulation
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

			// Normalize and orthogonalize
			for (auto& vertex : vertices)
			{
				glm::vec3 n = vertex.Normal;
				glm::vec3 t = vertex.Tangent;

				// Gram-Schmidt orthogonalize
				t = glm::normalize(t - n * glm::dot(n, t));

				// Calculate handedness
				if (glm::dot(glm::cross(n, t), vertex.Bitangent) < 0.0f)
				{
					t = t * -1.0f;
				}

				vertex.Tangent = t;
				vertex.Bitangent = glm::cross(n, t);
			}
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

			// Load diffuse/albedo maps
			std::vector<MeshTexture> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

			// Load specular maps
			std::vector<MeshTexture> specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

			// Load metallic maps
			std::vector<MeshTexture> metallicMaps = LoadMaterialTextures(material, aiTextureType_METALNESS, "texture_metallic");
			if (metallicMaps.empty()) {
				// Some formats store metallic in specular channel
				metallicMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_metallic");
			}
			textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());

			// Load roughness maps
			std::vector<MeshTexture> roughnessMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "texture_roughness");
			if (roughnessMaps.empty()) {
				// Some formats store roughness in shininess channel
				roughnessMaps = LoadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness");
			}
			textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

			// Load normal maps
			std::vector<MeshTexture> normalMaps = LoadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
			if (normalMaps.empty()) {
				normalMaps = LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
			}
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

			// Load AO maps
			std::vector<MeshTexture> aoMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT_OCCLUSION, "texture_ao");
			if (aoMaps.empty()) {
				aoMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_ao");
			}
			textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

			// Load emissive maps
			std::vector<MeshTexture> emissiveMaps = LoadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_emissive");
			textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
		}

		LNX_LOG_TRACE("Mesh created with {0} vertices, {1} indices, {2} textures",
			vertices.size(), indices.size(), textures.size());

		Ref<Mesh> result = CreateRef<Mesh>(vertices, indices, textures);

		// Skinning weights: keep the 4 strongest influences per vertex, normalized
		if (mesh->HasBones() && m_Skeleton) {
			std::vector<glm::ivec4> boneIDs(vertices.size(), glm::ivec4(0));
			std::vector<glm::vec4> boneWeights(vertices.size(), glm::vec4(0.0f));

			for (uint32_t b = 0; b < mesh->mNumBones; b++) {
				const aiBone* bone = mesh->mBones[b];
				auto it = m_Skeleton->BoneIndexByName.find(bone->mName.C_Str());
				if (it == m_Skeleton->BoneIndexByName.end() || it->second >= MAX_BONES) continue;
				int boneIndex = it->second;

				for (uint32_t w = 0; w < bone->mNumWeights; w++) {
					uint32_t v = bone->mWeights[w].mVertexId;
					float weight = bone->mWeights[w].mWeight;
					if (v >= vertices.size() || weight <= 0.0f) continue;

					// replace the weakest slot if this influence is stronger
					int slot = 0;
					for (int s = 1; s < MAX_BONE_INFLUENCES; s++)
						if (boneWeights[v][s] < boneWeights[v][slot]) slot = s;
					if (weight > boneWeights[v][slot]) {
						boneWeights[v][slot] = weight;
						boneIDs[v][slot] = boneIndex;
					}
				}
			}

			for (auto& w : boneWeights) {
				float sum = w.x + w.y + w.z + w.w;
				w = (sum > 0.0f) ? w / sum : glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			}

			result->SetBoneData(boneIDs, boneWeights);
		}

		return result;
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
				auto preloaded = m_PreloadedTextures.find(str.C_Str());
				texture.Texture = (preloaded != m_PreloadedTextures.end())
					? preloaded->second
					: Texture2D::Create(texturePath);
				texture.Type = typeName;
				texture.Path = str.C_Str();

				if (texture.Texture && texture.Texture->IsLoaded()) {
					textures.push_back(texture);
					m_TexturesLoaded.push_back(texture);
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
