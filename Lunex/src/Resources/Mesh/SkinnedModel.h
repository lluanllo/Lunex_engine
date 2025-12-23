#pragma once

/**
 * @file SkinnedModel.h
 * @brief Skinned 3D model with bone weights for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * This class extends Model to support bone weight data extraction
 * from FBX/GLTF files using Assimp.
 */

#include "Core/Core.h"
#include "SkinnedMesh.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;
enum aiTextureType;

namespace Lunex {

	// ============================================================================
	// BONE INFO (for mapping bone names to indices)
	// ============================================================================
	struct BoneInfo {
		int ID = -1;                          // Index in final bone array
		glm::mat4 OffsetMatrix = glm::mat4(1.0f);  // Inverse bind pose matrix
	};

	// ============================================================================
	// SKINNED MODEL
	// ============================================================================
	class SkinnedModel {
	public:
		SkinnedModel() = default;
		SkinnedModel(const std::string& path);
		~SkinnedModel() = default;

		void LoadModel(const std::string& path);
		void Draw(const Ref<Shader>& shader);
		
		// Validation
		bool IsValid() const { return !m_Meshes.empty(); }
		
		// Mesh access
		const std::vector<Ref<SkinnedMesh>>& GetMeshes() const { return m_Meshes; }
		
		// Bone data access
		const std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() const { return m_BoneInfoMap; }
		int GetBoneCount() const { return m_BoneCounter; }
		bool HasBones() const { return m_BoneCounter > 0; }
		
		// Get inverse bind pose matrices for all bones
		std::vector<glm::mat4> GetInverseBindPoseMatrices() const;
		
		// Get bone index by name (-1 if not found)
		int GetBoneIndex(const std::string& boneName) const;
		
		// Entity ID for picking
		void SetEntityID(int entityID);

		// Static factory for primitives (no bones)
		static Ref<SkinnedModel> CreateCube();
		static Ref<SkinnedModel> CreateSphere(uint32_t segments = 32);
		static Ref<SkinnedModel> CreatePlane();

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);
		Ref<SkinnedMesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<MeshTexture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);
		
		// Bone extraction
		void ExtractBoneWeights(std::vector<SkinnedVertex>& vertices, aiMesh* mesh, const aiScene* scene);
		void SetVertexBoneDataToDefault(SkinnedVertex& vertex);

	private:
		std::vector<Ref<SkinnedMesh>> m_Meshes;
		std::vector<MeshTexture> m_TexturesLoaded;
		std::string m_Directory;
		
		// Bone data
		std::unordered_map<std::string, BoneInfo> m_BoneInfoMap;
		int m_BoneCounter = 0;
	};

}
