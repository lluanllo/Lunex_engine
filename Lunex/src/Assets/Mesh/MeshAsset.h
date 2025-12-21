#pragma once

/**
 * @file MeshAsset.h
 * @brief Mesh asset for 3D models
 * 
 * Part of the Unified Assets System
 */

#include "Assets/Core/Asset.h"
#include "Resources/Mesh/Model.h"
#include "Resources/Mesh/Mesh.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// MESH IMPORT SETTINGS
	// ============================================================================
	struct MeshImportSettings {
		float Scale = 1.0f;
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		
		bool FlipUVs = false;
		bool GenerateNormals = true;
		bool GenerateTangents = true;
		bool OptimizeMesh = true;
		
		bool GenerateLODs = false;
		int LODLevels = 3;
		float LODReductionFactor = 0.5f;
		
		bool GenerateCollision = false;
		bool UseConvexCollision = true;
	};

	// ============================================================================
	// MESH METADATA
	// ============================================================================
	struct MeshMetadata {
		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;
		uint32_t TriangleCount = 0;
		uint32_t SubmeshCount = 0;
		
		glm::vec3 BoundsMin = { 0.0f, 0.0f, 0.0f };
		glm::vec3 BoundsMax = { 0.0f, 0.0f, 0.0f };
		glm::vec3 BoundsCenter = { 0.0f, 0.0f, 0.0f };
		float BoundsRadius = 0.0f;
		
		size_t VertexBufferSize = 0;
		size_t IndexBufferSize = 0;
		
		std::vector<std::string> MaterialNames;
	};

	// ============================================================================
	// MESH ASSET (.lumesh)
	// ============================================================================
	class MeshAsset : public Asset {
	public:
		MeshAsset();
		MeshAsset(const std::string& name);
		~MeshAsset() = default;

		// Asset interface
		AssetType GetType() const override { return AssetType::Mesh; }
		static AssetType StaticType() { return AssetType::Mesh; }

		// Source file
		bool HasValidSource() const;
		bool NeedsReimport() const;
		
		// Import settings
		const MeshImportSettings& GetImportSettings() const { return m_ImportSettings; }
		void SetImportSettings(const MeshImportSettings& settings) { m_ImportSettings = settings; MarkDirty(); }

		// Mesh metadata
		const MeshMetadata& GetMetadata() const { return m_Metadata; }
		
		uint32_t GetVertexCount() const { return m_Metadata.VertexCount; }
		uint32_t GetTriangleCount() const { return m_Metadata.TriangleCount; }
		uint32_t GetSubmeshCount() const { return m_Metadata.SubmeshCount; }
		
		glm::vec3 GetBoundsMin() const { return m_Metadata.BoundsMin; }
		glm::vec3 GetBoundsMax() const { return m_Metadata.BoundsMax; }
		glm::vec3 GetBoundsCenter() const { return m_Metadata.BoundsCenter; }
		float GetBoundsRadius() const { return m_Metadata.BoundsRadius; }

		// Runtime model
		Ref<Model> GetModel();
		bool IsModelLoaded() const { return m_Model != nullptr; }
		void ReloadModel();
		void UnloadModel();

		// Serialization
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<MeshAsset> LoadFromFile(const std::filesystem::path& path);

		// Import
		static Ref<MeshAsset> Import(const std::filesystem::path& sourcePath, 
									 const MeshImportSettings& settings = MeshImportSettings());
		bool Reimport();

	private:
		void CalculateMetadata();
		void SerializeImportSettings(YAML::Emitter& out) const;
		void SerializeMetadata(YAML::Emitter& out) const;
		void DeserializeImportSettings(const YAML::Node& node);
		void DeserializeMetadata(const YAML::Node& node);

	private:
		MeshImportSettings m_ImportSettings;
		MeshMetadata m_Metadata;
		std::filesystem::file_time_type m_SourceLastModified;
		Ref<Model> m_Model;
	};

}
