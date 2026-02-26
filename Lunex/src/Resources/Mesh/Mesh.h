#pragma once

/**
 * @file Mesh.h
 * @brief GPU mesh resource
 *
 * AAA Architecture: Mesh lives in Resources/Mesh/
 * This is the GPU-ready mesh with vertex/index buffers.
 */

#include "Core/Core.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Lunex {

	/**
	 * @struct Vertex
	 * @brief Standard vertex format (EntityID moved to Transform UBO for performance)
	 */
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
	};

	/**
	 * @struct MeshTexture
	 * @brief Texture reference for mesh rendering
	 */
	struct MeshTexture {
		Ref<Texture2D> Texture;
		std::string Type;
		std::string Path;
	};

	/**
	 * @struct MeshMaterialData
	 * @brief Per-mesh PBR material properties extracted from GLTF/Assimp
	 */
	struct MeshMaterialData {
		glm::vec4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		glm::vec3 EmissionColor = { 0.0f, 0.0f, 0.0f };
		float EmissionIntensity = 0.0f;
	};

	/**
	 * @brief Bitmask flags for fast texture type queries
	 */
	enum MeshTextureFlags : uint32_t {
		MeshTexFlag_None      = 0,
		MeshTexFlag_Diffuse   = 1 << 0,
		MeshTexFlag_Normal    = 1 << 1,
		MeshTexFlag_Metallic  = 1 << 2,
		MeshTexFlag_Roughness = 1 << 3,
		MeshTexFlag_Specular  = 1 << 4,
		MeshTexFlag_Emissive  = 1 << 5,
		MeshTexFlag_AO        = 1 << 6,
	};

	/**
	 * @class Mesh
	 * @brief GPU-ready mesh with vertex/index buffers
	 */
	class Mesh {
	public:
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
			const std::vector<MeshTexture>& textures = {},
			const MeshMaterialData& materialData = {});
		~Mesh() = default;

		void Draw(const Ref<Shader>& shader);
		void DrawGeometry(const Ref<Shader>& shader);

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		const std::vector<MeshTexture>& GetTextures() const { return m_Textures; }
		const MeshMaterialData& GetMaterialData() const { return m_MaterialData; }

		uint32_t GetIndexCount() const { return m_IndexCount; }

		bool HasMeshTexture(const std::string& type) const;
		bool HasAnyMeshTextures() const { return m_TextureFlags != MeshTexFlag_None; }
		uint32_t GetTextureFlags() const { return m_TextureFlags; }

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }

	private:
		void SetupMesh();
		void CacheTextureFlags();

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<MeshTexture> m_Textures;
		MeshMaterialData m_MaterialData;
		uint32_t m_TextureFlags = MeshTexFlag_None;
		uint32_t m_IndexCount = 0;

		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
	};

} // namespace Lunex} // namespace Lunex