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

		// Factory methods for primitives
		static Ref<Model> CreateCube();
		static Ref<Model> CreateSphere(uint32_t segments = 32);
		static Ref<Model> CreatePlane();
		static Ref<Model> CreateCylinder(uint32_t segments = 32);

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);
		Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<MeshTexture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);

	private:
		std::vector<Ref<Mesh>> m_Meshes;
		std::string m_Directory;
		std::vector<MeshTexture> m_TexturesLoaded;
	};

} // namespace Lunex
