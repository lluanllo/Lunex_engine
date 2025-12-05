#pragma once

#include "MeshAsset.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	// ============================================================================
	// MESH IMPORT RESULT
	// Result of a mesh import operation
	// ============================================================================
	struct MeshImportResult {
		bool Success = false;
		std::string ErrorMessage;
		Ref<MeshAsset> Asset;
		std::filesystem::path OutputPath;
	};

	// ============================================================================
	// MESH IMPORTER
	// Utility class for importing 3D model files and creating MeshAsset files
	// ============================================================================
	class MeshImporter {
	public:
		// Supported source file formats
		static bool IsSupported(const std::filesystem::path& path);
		static std::vector<std::string> GetSupportedExtensions();
		
		// ========== IMPORT ==========
		
		// Import a 3D model and create a .lumesh asset
		// @param sourcePath: Path to the source model file (.obj, .fbx, .gltf, etc.)
		// @param outputDir: Directory where to save the .lumesh file (optional, defaults to same as source)
		// @param settings: Import configuration
		// @return Import result containing success status and the created asset
		static MeshImportResult Import(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir = "",
			const MeshImportSettings& settings = MeshImportSettings()
		);
		
		// Import with custom output name
		static MeshImportResult ImportAs(
			const std::filesystem::path& sourcePath,
			const std::string& assetName,
			const std::filesystem::path& outputDir,
			const MeshImportSettings& settings = MeshImportSettings()
		);
		
		// ========== BATCH IMPORT ==========
		
		using ProgressCallback = std::function<void(const std::string& currentFile, int current, int total)>;
		
		// Import multiple files
		static std::vector<MeshImportResult> ImportBatch(
			const std::vector<std::filesystem::path>& sourcePaths,
			const std::filesystem::path& outputDir,
			const MeshImportSettings& settings = MeshImportSettings(),
			ProgressCallback progressCallback = nullptr
		);
		
		// Import all supported files in a directory
		static std::vector<MeshImportResult> ImportDirectory(
			const std::filesystem::path& sourceDir,
			const std::filesystem::path& outputDir,
			bool recursive = false,
			const MeshImportSettings& settings = MeshImportSettings(),
			ProgressCallback progressCallback = nullptr
		);
		
		// ========== REIMPORT ==========
		
		// Reimport an existing MeshAsset from its source file
		static bool Reimport(Ref<MeshAsset> asset);
		
		// Reimport with new settings
		static bool Reimport(Ref<MeshAsset> asset, const MeshImportSettings& settings);
		
		// ========== VALIDATION ==========
		
		// Validate a source file before importing
		static bool Validate(const std::filesystem::path& sourcePath, std::string& outError);
		
		// Get info about a model file without fully importing it
		struct ModelInfo {
			uint32_t MeshCount = 0;
			uint32_t TotalVertices = 0;
			uint32_t TotalTriangles = 0;
			std::vector<std::string> MaterialNames;
			bool HasAnimations = false;
			bool HasBones = false;
		};
		
		static ModelInfo GetModelInfo(const std::filesystem::path& sourcePath);
		
	private:
		// Generate output path for the .lumesh file
		static std::filesystem::path GenerateOutputPath(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir,
			const std::string& customName = ""
		);
	};

}
