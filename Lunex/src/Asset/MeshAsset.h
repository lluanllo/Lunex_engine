#pragma once

#include "Asset/Asset.h"
#include "Renderer/Model.h"
#include "Renderer/Mesh.h"

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
	// Configuration for how to import a 3D model file
	// ============================================================================
	struct MeshImportSettings {
		// Transform
		float Scale = 1.0f;
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };  // Euler angles in degrees
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		
		// Processing
		bool FlipUVs = false;
		bool GenerateNormals = true;
		bool GenerateTangents = true;
		bool OptimizeMesh = true;
		
		// LOD (future)
		bool GenerateLODs = false;
		int LODLevels = 3;
		float LODReductionFactor = 0.5f;
		
		// Collision (future)
		bool GenerateCollision = false;
		bool UseConvexCollision = true;
	};

	// ============================================================================
	// MESH METADATA
	// Cached information about the mesh (read-only after import)
	// ============================================================================
	struct MeshMetadata {
		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;
		uint32_t TriangleCount = 0;
		uint32_t SubmeshCount = 0;
		
		// Bounds
		glm::vec3 BoundsMin = { 0.0f, 0.0f, 0.0f };
		glm::vec3 BoundsMax = { 0.0f, 0.0f, 0.0f };
		glm::vec3 BoundsCenter = { 0.0f, 0.0f, 0.0f };
		float BoundsRadius = 0.0f;
		
		// Memory
		size_t VertexBufferSize = 0;
		size_t IndexBufferSize = 0;
		
		// Materials (embedded in source file)
		std::vector<std::string> MaterialNames;
	};

	// ============================================================================
	// MESH ASSET (.lumesh)
	// Represents a 3D mesh that can be shared across entities
	// Does NOT embed the actual mesh data - references the source file
	// ============================================================================
	class MeshAsset : public Asset {
	public:
		MeshAsset();
		MeshAsset(const std::string& name);
		~MeshAsset() = default;

		// ========== ASSET TYPE ==========
		AssetType GetType() const override { return AssetType::Mesh; }
		static AssetType GetStaticType() { return AssetType::Mesh; }

		// ========== SOURCE FILE ==========
		
		// Path to the original model file (.obj, .fbx, .gltf, etc.)
		// Note: Uses m_SourcePath from Asset base class
		
		// Check if source file exists
		bool HasValidSource() const;
		
		// Check if source file was modified (needs reimport)
		bool NeedsReimport() const;
		
		// ========== IMPORT SETTINGS ==========
		
		const MeshImportSettings& GetImportSettings() const { return m_ImportSettings; }
		void SetImportSettings(const MeshImportSettings& settings) { m_ImportSettings = settings; MarkDirty(); }

		// ========== MESH METADATA ==========
		
		const MeshMetadata& GetMetadata() const { return m_Metadata; }
		
		uint32_t GetVertexCount() const { return m_Metadata.VertexCount; }
		uint32_t GetTriangleCount() const { return m_Metadata.TriangleCount; }
		uint32_t GetSubmeshCount() const { return m_Metadata.SubmeshCount; }
		
		glm::vec3 GetBoundsMin() const { return m_Metadata.BoundsMin; }
		glm::vec3 GetBoundsMax() const { return m_Metadata.BoundsMax; }
		glm::vec3 GetBoundsCenter() const { return m_Metadata.BoundsCenter; }
		float GetBoundsRadius() const { return m_Metadata.BoundsRadius; }

		// ========== RUNTIME MODEL ==========
		
		// Get the loaded Model (loads from source if not already loaded)
		Ref<Model> GetModel();
		
		// Check if model is currently loaded in memory
		bool IsModelLoaded() const { return m_Model != nullptr; }
		
		// Force reload from source file
		void ReloadModel();
		
		// Unload model from memory (keeps asset data)
		void UnloadModel();

		// ========== SERIALIZATION ==========
		
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<MeshAsset> LoadFromFile(const std::filesystem::path& path);

		// ========== IMPORT ==========
		
		// Import from a source model file
		static Ref<MeshAsset> Import(const std::filesystem::path& sourcePath, 
									 const MeshImportSettings& settings = MeshImportSettings());
		
		// Reimport with current settings
		bool Reimport();

	private:
		// Calculate metadata from loaded model
		void CalculateMetadata();
		
		// Serialize helpers
		void SerializeImportSettings(YAML::Emitter& out) const;
		void SerializeMetadata(YAML::Emitter& out) const;
		void DeserializeImportSettings(const YAML::Node& node);
		void DeserializeMetadata(const YAML::Node& node);

	private:
		// Import configuration
		MeshImportSettings m_ImportSettings;
		
		// Cached metadata (from last import)
		MeshMetadata m_Metadata;
		
		// Source file timestamp (for change detection)
		std::filesystem::file_time_type m_SourceLastModified;
		
		// Runtime model (loaded on demand)
		Ref<Model> m_Model;
	};

}
