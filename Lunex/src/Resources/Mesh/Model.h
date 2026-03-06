#pragma once

/**
 * @file Model.h
 * @brief GPU model resource (collection of meshes)
 * 
 * AAA Architecture: Model lives in Resources/Mesh/
 * This is the GPU-ready model with multiple meshes.
 */

#include "Mesh.h"
#include "Core/Core.h"

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

// Forward declarations
struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;
enum aiTextureType : int;

namespace Lunex {

	/**
	 * @enum ModelType
	 * @brief Type of model source
	 */
	enum class ModelType {
		Cube,
		Sphere,
		Plane,
		Cylinder,
		FromFile
	};

	/**
	 * @struct ModelMaterialData
	 * @brief Extracted PBR material data from an imported model (per sub-mesh)
	 */
	struct ModelMaterialData {
		std::string Name = "Default";

		// PBR properties
		glm::vec4 AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		glm::vec3 EmissionColor = { 0.0f, 0.0f, 0.0f };
		float EmissionIntensity = 0.0f;

		// Texture paths (absolute paths, ready to load)
		std::string AlbedoTexturePath;
		std::string NormalTexturePath;
		std::string MetallicTexturePath;
		std::string RoughnessTexturePath;
		std::string AOTexturePath;
		std::string EmissionTexturePath;

		// GLTF metallic-roughness combined texture
		std::string MetallicRoughnessTexturePath;
		bool HasMetallicRoughnessMap = false;
	};

	/**
	 * @class Model
	 * @brief Collection of meshes forming a 3D model
	 */
	class Model {
	public:
		Model() = default;
		Model(const std::string& path);
		~Model() = default;

		void LoadModel(const std::string& path);
		void Draw(const Ref<Shader>& shader);
		void SetEntityID(int entityID);

		const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; }
		const std::string& GetDirectory() const { return m_Directory; }

		// Per-mesh material data extracted during import
		const std::vector<ModelMaterialData>& GetMaterialData() const { return m_MaterialData; }
		bool HasMaterialData() const { return !m_MaterialData.empty(); }

		// Material index per submesh (maps each mesh to its unique material index in m_MaterialData)
		const std::vector<uint32_t>& GetSubmeshMaterialIndices() const { return m_SubmeshMaterialIndices; }

		// Factory methods for primitives
		static Ref<Model> CreateCube();
		static Ref<Model> CreateSphere(uint32_t segments = 32);
		static Ref<Model> CreatePlane();
		static Ref<Model> CreateCylinder(uint32_t segments = 32);

	private:
		void ProcessNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
		Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform);
		std::vector<MeshTexture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName, const aiScene* scene);
		ModelMaterialData ExtractMaterialData(aiMaterial* material, const aiScene* scene);
		std::string ResolveTexturePath(const std::string& texPath, const aiScene* scene);

	private:
		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<ModelMaterialData> m_MaterialData;
		std::vector<uint32_t> m_SubmeshMaterialIndices; // Maps each submesh to index in m_MaterialData
		std::string m_Directory;
		std::vector<MeshTexture> m_TexturesLoaded;
		
		// Temporary: used during LoadModel to map Assimp material indices to m_MaterialData indices
		std::unordered_map<unsigned int, uint32_t> m_AssimpMatToIndex;
	};

} // namespace Lunex
