#pragma once

#include "Core/Core.h"
#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <vector>

namespace Lunex {

	enum class ModelType {
		Cube,
		Sphere,
		Plane,
		Cylinder,
		FromFile
	};

	class Model {
	public:
		Model() = default;
		Model(const std::string& path);
		~Model() = default;

		void LoadModel(const std::string& path);
		void Draw(const Ref<Shader>& shader);

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
		std::vector<MeshTexture> m_TexturesLoaded;  // Para evitar cargar texturas duplicadas
	};

}
