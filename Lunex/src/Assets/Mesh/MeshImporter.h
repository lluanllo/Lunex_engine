#pragma once

/**
 * @file MeshImporter.h
 * @brief Mesh import utilities
 * 
 * Part of the Unified Assets System
 */

#include "MeshAsset.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	// ============================================================================
	// MESH IMPORT RESULT
	// ============================================================================
	struct MeshImportResult {
		bool Success = false;
		std::string ErrorMessage;
		Ref<MeshAsset> Asset;
		std::filesystem::path OutputPath;
	};

	// ============================================================================
	// MESH IMPORTER
	// ============================================================================
	class MeshImporter {
	public:
		// Supported source file formats
		static bool IsSupported(const std::filesystem::path& path);
		static std::vector<std::string> GetSupportedExtensions();
		
		// Import
		static MeshImportResult Import(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir = "",
			const MeshImportSettings& settings = MeshImportSettings()
		);
		
		static MeshImportResult ImportAs(
			const std::filesystem::path& sourcePath,
			const std::string& assetName,
			const std::filesystem::path& outputDir,
			const MeshImportSettings& settings = MeshImportSettings()
		);
		
		// Batch import
		using ProgressCallback = std::function<void(const std::string& currentFile, int current, int total)>;
		
		static std::vector<MeshImportResult> ImportBatch(
			const std::vector<std::filesystem::path>& sourcePaths,
			const std::filesystem::path& outputDir,
			const MeshImportSettings& settings = MeshImportSettings(),
			ProgressCallback progressCallback = nullptr
		);
		
		static std::vector<MeshImportResult> ImportDirectory(
			const std::filesystem::path& sourceDir,
			const std::filesystem::path& outputDir,
			bool recursive = false,
			const MeshImportSettings& settings = MeshImportSettings(),
			ProgressCallback progressCallback = nullptr
		);
		
		// Reimport
		static bool Reimport(Ref<MeshAsset> asset);
		static bool Reimport(Ref<MeshAsset> asset, const MeshImportSettings& settings);
		
		// Validation
		static bool Validate(const std::filesystem::path& sourcePath, std::string& outError);
		
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
		static std::filesystem::path GenerateOutputPath(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir,
			const std::string& customName = ""
		);
	};

}
