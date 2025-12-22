#pragma once

/**
 * @file AnimationImporter.h
 * @brief Animation import utilities for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * Imports skeletons and animation clips from:
 *   - glTF/GLB (recommended)
 *   - FBX
 *   - DAE (Collada)
 *   - Other Assimp-supported formats
 */

#include "SkeletonAsset.h"
#include "AnimationClipAsset.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	// ============================================================================
	// ANIMATION IMPORT SETTINGS
	// ============================================================================
	
	struct AnimationImportSettings {
		// Skeleton settings
		bool ImportSkeleton = true;
		bool ComputeBindPoses = true;
		
		// Animation settings
		bool ImportAnimations = true;
		float SampleRate = 30.0f;           // Frames per second for resampling
		bool ResampleAnimations = false;    // Resample to fixed rate
		bool OptimizeKeyframes = true;      // Remove redundant keyframes
		float KeyframeThreshold = 0.0001f;  // Threshold for optimization
		
		// Transform settings
		float Scale = 1.0f;
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };    // Euler angles in degrees
		bool ConvertCoordinateSystem = true;  // Auto-convert from source
		
		// Naming
		std::string SkeletonNameOverride;   // Custom skeleton name
		std::string AnimationPrefix;        // Prefix for animation clips
	};

	// ============================================================================
	// ANIMATION IMPORT RESULT
	// ============================================================================
	
	struct AnimationImportResult {
		bool Success = false;
		std::string ErrorMessage;
		
		Ref<SkeletonAsset> Skeleton;
		std::vector<Ref<AnimationClipAsset>> Clips;
		
		std::filesystem::path SkeletonOutputPath;
		std::vector<std::filesystem::path> ClipOutputPaths;
	};

	// ============================================================================
	// ANIMATION IMPORTER
	// ============================================================================
	
	class AnimationImporter {
	public:
		// Supported source file formats
		static bool IsSupported(const std::filesystem::path& path);
		static std::vector<std::string> GetSupportedExtensions();
		
		// ========== FULL IMPORT ==========
		
		/**
		 * Import skeleton and all animations from a model file
		 */
		static AnimationImportResult Import(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir = "",
			const AnimationImportSettings& settings = AnimationImportSettings()
		);
		
		// ========== SELECTIVE IMPORT ==========
		
		/**
		 * Import only the skeleton
		 */
		static Ref<SkeletonAsset> ImportSkeleton(
			const std::filesystem::path& sourcePath,
			const AnimationImportSettings& settings = AnimationImportSettings()
		);
		
		/**
		 * Import only animations (requires existing skeleton for joint resolution)
		 */
		static std::vector<Ref<AnimationClipAsset>> ImportAnimations(
			const std::filesystem::path& sourcePath,
			Ref<SkeletonAsset> skeleton = nullptr,
			const AnimationImportSettings& settings = AnimationImportSettings()
		);
		
		/**
		 * Import a specific animation by name
		 */
		static Ref<AnimationClipAsset> ImportAnimation(
			const std::filesystem::path& sourcePath,
			const std::string& animationName,
			Ref<SkeletonAsset> skeleton = nullptr,
			const AnimationImportSettings& settings = AnimationImportSettings()
		);
		
		// ========== BATCH IMPORT ==========
		
		using ProgressCallback = std::function<void(const std::string& currentFile, int current, int total)>;
		
		static std::vector<AnimationImportResult> ImportBatch(
			const std::vector<std::filesystem::path>& sourcePaths,
			const std::filesystem::path& outputDir,
			const AnimationImportSettings& settings = AnimationImportSettings(),
			ProgressCallback progressCallback = nullptr
		);
		
		// ========== UTILITIES ==========
		
		/**
		 * Get information about animations in a file without importing
		 */
		struct AnimationInfo {
			std::string Name;
			float Duration = 0.0f;
			float TicksPerSecond = 0.0f;
			uint32_t ChannelCount = 0;
		};
		
		struct ModelAnimationInfo {
			bool HasSkeleton = false;
			uint32_t BoneCount = 0;
			std::vector<AnimationInfo> Animations;
		};
		
		static ModelAnimationInfo GetAnimationInfo(const std::filesystem::path& sourcePath);
		
		/**
		 * Validate a source file for animation import
		 */
		static bool Validate(const std::filesystem::path& sourcePath, std::string& outError);
		
	private:
		static std::filesystem::path GenerateSkeletonPath(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir,
			const std::string& customName = ""
		);
		
		static std::filesystem::path GenerateAnimationPath(
			const std::filesystem::path& sourcePath,
			const std::filesystem::path& outputDir,
			const std::string& animationName,
			const std::string& prefix = ""
		);
	};

}
