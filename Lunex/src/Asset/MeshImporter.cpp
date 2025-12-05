#include "stpch.h"
#include "MeshImporter.h"
#include "Log/Log.h"

// Assimp for model info
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Lunex {

	// Supported file extensions
	static const std::vector<std::string> s_SupportedExtensions = {
		".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl"
	};

	bool MeshImporter::IsSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		for (const auto& supported : s_SupportedExtensions) {
			if (ext == supported) return true;
		}
		return false;
	}

	std::vector<std::string> MeshImporter::GetSupportedExtensions() {
		return s_SupportedExtensions;
	}

	MeshImportResult MeshImporter::Import(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const MeshImportSettings& settings)
	{
		return ImportAs(sourcePath, "", outputDir, settings);
	}

	MeshImportResult MeshImporter::ImportAs(
		const std::filesystem::path& sourcePath,
		const std::string& assetName,
		const std::filesystem::path& outputDir,
		const MeshImportSettings& settings)
	{
		MeshImportResult result;
		
		// Validate source file
		if (!std::filesystem::exists(sourcePath)) {
			result.ErrorMessage = "Source file not found: " + sourcePath.string();
			LNX_LOG_ERROR("MeshImporter: {0}", result.ErrorMessage);
			return result;
		}

		if (!IsSupported(sourcePath)) {
			result.ErrorMessage = "Unsupported file format: " + sourcePath.extension().string();
			LNX_LOG_ERROR("MeshImporter: {0}", result.ErrorMessage);
			return result;
		}

		// Validate before import
		std::string validationError;
		if (!Validate(sourcePath, validationError)) {
			result.ErrorMessage = validationError;
			return result;
		}

		// Create the mesh asset
		Ref<MeshAsset> meshAsset = MeshAsset::Import(sourcePath, settings);
		if (!meshAsset) {
			result.ErrorMessage = "Failed to import model from: " + sourcePath.string();
			LNX_LOG_ERROR("MeshImporter: {0}", result.ErrorMessage);
			return result;
		}

		// Set custom name if provided
		if (!assetName.empty()) {
			meshAsset->SetName(assetName);
		}

		// Generate output path
		result.OutputPath = GenerateOutputPath(sourcePath, outputDir, assetName);

		// Ensure output directory exists
		std::filesystem::path outputDirectory = result.OutputPath.parent_path();
		if (!outputDirectory.empty() && !std::filesystem::exists(outputDirectory)) {
			std::filesystem::create_directories(outputDirectory);
		}

		// Save the mesh asset
		if (!meshAsset->SaveToFile(result.OutputPath)) {
			result.ErrorMessage = "Failed to save mesh asset to: " + result.OutputPath.string();
			LNX_LOG_ERROR("MeshImporter: {0}", result.ErrorMessage);
			return result;
		}

		result.Success = true;
		result.Asset = meshAsset;
		
		LNX_LOG_INFO("MeshImporter: Successfully imported '{0}' -> '{1}'", 
			sourcePath.filename().string(), result.OutputPath.filename().string());

		return result;
	}

	std::vector<MeshImportResult> MeshImporter::ImportBatch(
		const std::vector<std::filesystem::path>& sourcePaths,
		const std::filesystem::path& outputDir,
		const MeshImportSettings& settings,
		ProgressCallback progressCallback)
	{
		std::vector<MeshImportResult> results;
		results.reserve(sourcePaths.size());

		int total = static_cast<int>(sourcePaths.size());
		int current = 0;

		for (const auto& sourcePath : sourcePaths) {
			current++;
			
			if (progressCallback) {
				progressCallback(sourcePath.filename().string(), current, total);
			}

			MeshImportResult result = Import(sourcePath, outputDir, settings);
			results.push_back(result);
		}

		// Log summary
		int successCount = 0;
		for (const auto& result : results) {
			if (result.Success) successCount++;
		}

		LNX_LOG_INFO("MeshImporter: Batch import complete. {0}/{1} successful", successCount, total);

		return results;
	}

	std::vector<MeshImportResult> MeshImporter::ImportDirectory(
		const std::filesystem::path& sourceDir,
		const std::filesystem::path& outputDir,
		bool recursive,
		const MeshImportSettings& settings,
		ProgressCallback progressCallback)
	{
		std::vector<std::filesystem::path> sourcePaths;

		if (recursive) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
				if (entry.is_regular_file() && IsSupported(entry.path())) {
					sourcePaths.push_back(entry.path());
				}
			}
		}
		else {
			for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
				if (entry.is_regular_file() && IsSupported(entry.path())) {
					sourcePaths.push_back(entry.path());
				}
			}
		}

		return ImportBatch(sourcePaths, outputDir, settings, progressCallback);
	}

	bool MeshImporter::Reimport(Ref<MeshAsset> asset) {
		if (!asset) return false;
		return asset->Reimport();
	}

	bool MeshImporter::Reimport(Ref<MeshAsset> asset, const MeshImportSettings& settings) {
		if (!asset) return false;
		
		asset->SetImportSettings(settings);
		return asset->Reimport();
	}

	bool MeshImporter::Validate(const std::filesystem::path& sourcePath, std::string& outError) {
		if (!std::filesystem::exists(sourcePath)) {
			outError = "File not found: " + sourcePath.string();
			return false;
		}

		if (!IsSupported(sourcePath)) {
			outError = "Unsupported format: " + sourcePath.extension().string();
			return false;
		}

		// Try to load with Assimp to validate
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), aiProcess_ValidateDataStructure);

		if (!scene) {
			outError = "Failed to parse file: " + std::string(importer.GetErrorString());
			return false;
		}

		if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
			outError = "Model is incomplete or corrupted";
			return false;
		}

		if (scene->mNumMeshes == 0) {
			outError = "Model contains no meshes";
			return false;
		}

		return true;
	}

	MeshImporter::ModelInfo MeshImporter::GetModelInfo(const std::filesystem::path& sourcePath) {
		ModelInfo info;

		if (!std::filesystem::exists(sourcePath)) {
			return info;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), 
			aiProcess_Triangulate | aiProcess_ValidateDataStructure);

		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
			return info;
		}

		info.MeshCount = scene->mNumMeshes;
		info.HasAnimations = scene->mNumAnimations > 0;

		for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
			const aiMesh* mesh = scene->mMeshes[i];
			info.TotalVertices += mesh->mNumVertices;
			info.TotalTriangles += mesh->mNumFaces;
			
			if (mesh->HasBones()) {
				info.HasBones = true;
			}
		}

		for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
			aiString name;
			scene->mMaterials[i]->Get(AI_MATKEY_NAME, name);
			info.MaterialNames.push_back(name.C_Str());
		}

		return info;
	}

	std::filesystem::path MeshImporter::GenerateOutputPath(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const std::string& customName)
	{
		// Determine output directory
		std::filesystem::path outDir = outputDir;
		if (outDir.empty()) {
			outDir = sourcePath.parent_path();
		}

		// Determine filename
		std::string filename;
		if (!customName.empty()) {
			filename = customName;
		}
		else {
			filename = sourcePath.stem().string();
		}

		// Add .lumesh extension
		return outDir / (filename + ".lumesh");
	}

}
