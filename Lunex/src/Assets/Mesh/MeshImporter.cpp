#include "stpch.h"
#include "MeshImporter.h"
#include "Assets/Animation/AnimationImporter.h"
#include "Log/Log.h"

#include <algorithm>

namespace Lunex {

	std::vector<std::string> MeshImporter::GetSupportedExtensions() {
		return { ".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds", ".ply", ".stl" };
	}
	
	bool MeshImporter::IsSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		auto supported = GetSupportedExtensions();
		return std::find(supported.begin(), supported.end(), ext) != supported.end();
	}
	
	MeshImportResult MeshImporter::Import(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const MeshImportSettings& settings)
	{
		MeshImportResult result;
		
		if (!std::filesystem::exists(sourcePath)) {
			result.ErrorMessage = "Source file not found: " + sourcePath.string();
			return result;
		}
		
		if (!IsSupported(sourcePath)) {
			result.ErrorMessage = "Unsupported file format: " + sourcePath.extension().string();
			return result;
		}
		
		// Import using MeshAsset
		result.Asset = MeshAsset::Import(sourcePath, settings);
		
		if (result.Asset) {
			result.Success = true;
			
			// Determine output path
			std::filesystem::path outDir = outputDir.empty() ? sourcePath.parent_path() : outputDir;
			result.OutputPath = GenerateOutputPath(sourcePath, outDir);
			
			// Save the asset
			result.Asset->SetPath(result.OutputPath);
			result.Asset->Save();
			
			// ? AUTO-IMPORT ANIMATIONS if the mesh has bones
			// Check if source file contains skeleton/animations
			auto animInfo = AnimationImporter::GetAnimationInfo(sourcePath);
			
			if (animInfo.HasSkeleton && animInfo.BoneCount > 0) {
				LNX_LOG_INFO("Detected skeletal mesh with {0} bones. Importing skeleton and animations...", 
					animInfo.BoneCount);
				
				// Configure animation import settings
				AnimationImportSettings animSettings;
				animSettings.ImportSkeleton = true;
				animSettings.ImportAnimations = true;
				animSettings.OptimizeKeyframes = true;
				animSettings.Scale = settings.Scale;
				
				// Import skeleton and animations
				auto animResult = AnimationImporter::Import(sourcePath, outDir, animSettings);
				
				if (animResult.Success) {
					LNX_LOG_INFO("Imported skeleton: {0}", animResult.SkeletonOutputPath.string());
					
					for (size_t i = 0; i < animResult.Clips.size(); i++) {
						LNX_LOG_INFO("Imported animation clip: {0}", animResult.ClipOutputPaths[i].string());
					}
					
					if (animResult.Clips.empty()) {
						LNX_LOG_INFO("No animations found in file. Skeleton-only import complete.");
					}
				} else {
					LNX_LOG_WARN("Failed to import animations: {0}", animResult.ErrorMessage);
				}
			}
		} else {
			result.ErrorMessage = "Failed to import mesh from: " + sourcePath.string();
		}
		
		return result;
	}
	
	MeshImportResult MeshImporter::ImportAs(
		const std::filesystem::path& sourcePath,
		const std::string& assetName,
		const std::filesystem::path& outputDir,
		const MeshImportSettings& settings)
	{
		MeshImportResult result = Import(sourcePath, outputDir, settings);
		
		if (result.Success && result.Asset) {
			result.Asset->SetName(assetName);
			result.OutputPath = GenerateOutputPath(sourcePath, outputDir, assetName);
			result.Asset->SetPath(result.OutputPath);
			result.Asset->Save();
		}
		
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
		
		for (size_t i = 0; i < sourcePaths.size(); ++i) {
			if (progressCallback) {
				progressCallback(sourcePaths[i].filename().string(), 
								 static_cast<int>(i + 1), 
								 static_cast<int>(sourcePaths.size()));
			}
			
			results.push_back(Import(sourcePaths[i], outputDir, settings));
		}
		
		return results;
	}
	
	std::vector<MeshImportResult> MeshImporter::ImportDirectory(
		const std::filesystem::path& sourceDir,
		const std::filesystem::path& outputDir,
		bool recursive,
		const MeshImportSettings& settings,
		ProgressCallback progressCallback)
	{
		std::vector<std::filesystem::path> files;
		
		if (recursive) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
				if (entry.is_regular_file() && IsSupported(entry.path())) {
					files.push_back(entry.path());
				}
			}
		} else {
			for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
				if (entry.is_regular_file() && IsSupported(entry.path())) {
					files.push_back(entry.path());
				}
			}
		}
		
		return ImportBatch(files, outputDir, settings, progressCallback);
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
			outError = "File not found";
			return false;
		}
		
		if (!IsSupported(sourcePath)) {
			outError = "Unsupported file format";
			return false;
		}
		
		return true;
	}
	
	MeshImporter::ModelInfo MeshImporter::GetModelInfo(const std::filesystem::path& sourcePath) {
		ModelInfo info;
		
		// This would use Assimp to get model info without full import
		// For now, return empty info
		
		return info;
	}
	
	std::filesystem::path MeshImporter::GenerateOutputPath(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const std::string& customName)
	{
		std::string name = customName.empty() ? sourcePath.stem().string() : customName;
		return outputDir / (name + ".lumesh");
	}

} // namespace Lunex
