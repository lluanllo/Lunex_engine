#include "stpch.h"
#include "AnimationImporter.h"
#include "Log/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <set>

namespace Lunex {

	// ============================================================================
	// HELPER FUNCTIONS
	// ============================================================================
	
	static glm::mat4 AssimpToGlmMatrix(const aiMatrix4x4& m) {
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	}
	
	static glm::vec3 AssimpToGlmVec3(const aiVector3D& v) {
		return glm::vec3(v.x, v.y, v.z);
	}
	
	static glm::quat AssimpToGlmQuat(const aiQuaternion& q) {
		return glm::quat(q.w, q.x, q.y, q.z);
	}
	
	static void DecomposeMatrix(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale) {
		position = glm::vec3(matrix[3]);
		
		scale.x = glm::length(glm::vec3(matrix[0]));
		scale.y = glm::length(glm::vec3(matrix[1]));
		scale.z = glm::length(glm::vec3(matrix[2]));
		
		glm::mat3 rotMat(
			glm::vec3(matrix[0]) / scale.x,
			glm::vec3(matrix[1]) / scale.y,
			glm::vec3(matrix[2]) / scale.z
		);
		rotation = glm::quat_cast(rotMat);
	}

	// ============================================================================
	// IMPORTER IMPLEMENTATION
	// ============================================================================
	
	std::vector<std::string> AnimationImporter::GetSupportedExtensions() {
		return { ".gltf", ".glb", ".fbx", ".dae", ".obj" };
	}
	
	bool AnimationImporter::IsSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		auto supported = GetSupportedExtensions();
		return std::find(supported.begin(), supported.end(), ext) != supported.end();
	}
	
	AnimationImportResult AnimationImporter::Import(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const AnimationImportSettings& settings)
	{
		AnimationImportResult result;
		
		if (!std::filesystem::exists(sourcePath)) {
			result.ErrorMessage = "Source file not found: " + sourcePath.string();
			return result;
		}
		
		// Import skeleton
		if (settings.ImportSkeleton) {
			result.Skeleton = ImportSkeleton(sourcePath, settings);
		}
		
		// Import animations
		if (settings.ImportAnimations) {
			result.Clips = ImportAnimations(sourcePath, result.Skeleton, settings);
		}
		
		// Save assets if output directory specified
		std::filesystem::path outDir = outputDir.empty() ? sourcePath.parent_path() : outputDir;
		
		if (result.Skeleton) {
			result.SkeletonOutputPath = GenerateSkeletonPath(sourcePath, outDir, settings.SkeletonNameOverride);
			result.Skeleton->SaveToFile(result.SkeletonOutputPath);
		}
		
		for (size_t i = 0; i < result.Clips.size(); i++) {
			auto& clip = result.Clips[i];
			auto clipPath = GenerateAnimationPath(sourcePath, outDir, clip->GetName(), settings.AnimationPrefix);
			clip->SaveToFile(clipPath);
			result.ClipOutputPaths.push_back(clipPath);
		}
		
		result.Success = (result.Skeleton != nullptr) || !result.Clips.empty();
		
		if (result.Success) {
			LNX_LOG_INFO("Animation import successful: {0}", sourcePath.string());
			if (result.Skeleton) {
				LNX_LOG_INFO("  Skeleton: {0} bones", result.Skeleton->GetJointCount());
			}
			LNX_LOG_INFO("  Animations: {0} clips", result.Clips.size());
		}
		
		return result;
	}
	
	Ref<SkeletonAsset> AnimationImporter::ImportSkeleton(
		const std::filesystem::path& sourcePath,
		const AnimationImportSettings& settings)
	{
		Assimp::Importer importer;
		
		unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), flags);
		
		if (!scene || !scene->mRootNode) {
			LNX_LOG_ERROR("AnimationImporter: Failed to load file: {0}", sourcePath.string());
			return nullptr;
		}
		
		// Check if scene has bones
		bool hasBones = false;
		for (unsigned int i = 0; i < scene->mNumMeshes && !hasBones; i++) {
			if (scene->mMeshes[i]->mNumBones > 0) {
				hasBones = true;
			}
		}
		
		if (!hasBones) {
			LNX_LOG_WARN("AnimationImporter: No bones found in: {0}", sourcePath.string());
			return nullptr;
		}
		
		auto skeleton = CreateRef<SkeletonAsset>();
		std::string name = settings.SkeletonNameOverride.empty() 
			? sourcePath.stem().string() 
			: settings.SkeletonNameOverride;
		skeleton->SetName(name);
		skeleton->SetSourcePath(sourcePath);
		
		// Build bone hierarchy
		std::unordered_map<std::string, int32_t> boneIndexMap;
		std::unordered_map<std::string, aiMatrix4x4> boneOffsets;
		
		// First pass: collect all bones from meshes
		for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
			aiMesh* mesh = scene->mMeshes[m];
			for (unsigned int b = 0; b < mesh->mNumBones; b++) {
				aiBone* bone = mesh->mBones[b];
				std::string boneName = bone->mName.C_Str();
				
				if (boneIndexMap.find(boneName) == boneIndexMap.end()) {
					boneIndexMap[boneName] = static_cast<int32_t>(boneIndexMap.size());
					boneOffsets[boneName] = bone->mOffsetMatrix;
				}
			}
		}
		
		// Second pass: build hierarchy from node tree
		std::function<void(aiNode*, int32_t)> processNode = [&](aiNode* node, int32_t parentIndex) {
			std::string nodeName = node->mName.C_Str();
			
			auto it = boneIndexMap.find(nodeName);
			if (it != boneIndexMap.end()) {
				SkeletonJoint joint;
				joint.Name = nodeName;
				joint.ParentIndex = parentIndex;
				
				// Get local transform
				glm::mat4 localTransform = AssimpToGlmMatrix(node->mTransformation);
				DecomposeMatrix(localTransform, joint.LocalPosition, joint.LocalRotation, joint.LocalScale);
				
				// Apply scale from settings
				joint.LocalPosition *= settings.Scale;
				
				// Get inverse bind pose from bone offset
				if (boneOffsets.find(nodeName) != boneOffsets.end()) {
					joint.InverseBindPose = AssimpToGlmMatrix(boneOffsets[nodeName]);
				}
				
				skeleton->AddJoint(joint);
				parentIndex = skeleton->GetJointCount() - 1;
			}
			
			for (unsigned int i = 0; i < node->mNumChildren; i++) {
				processNode(node->mChildren[i], parentIndex);
			}
		};
		
		processNode(scene->mRootNode, -1);
		
		// Compute bind poses
		if (settings.ComputeBindPoses) {
			skeleton->ComputeBindPoses();
			skeleton->ComputeInverseBindPoses();
		}
		
		skeleton->SetLoaded(true);
		
		LNX_LOG_INFO("Skeleton imported: {0} ({1} bones)", skeleton->GetName(), skeleton->GetJointCount());
		return skeleton;
	}
	
	std::vector<Ref<AnimationClipAsset>> AnimationImporter::ImportAnimations(
		const std::filesystem::path& sourcePath,
		Ref<SkeletonAsset> skeleton,
		const AnimationImportSettings& settings)
	{
		std::vector<Ref<AnimationClipAsset>> clips;
		
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), 0);
		
		if (!scene || scene->mNumAnimations == 0) {
			LNX_LOG_WARN("AnimationImporter: No animations found in: {0}", sourcePath.string());
			return clips;
		}
		
		for (unsigned int a = 0; a < scene->mNumAnimations; a++) {
			aiAnimation* anim = scene->mAnimations[a];
			
			auto clip = CreateRef<AnimationClipAsset>();
			
			std::string animName = anim->mName.length > 0 ? anim->mName.C_Str() : "Animation_" + std::to_string(a);
			if (!settings.AnimationPrefix.empty()) {
				animName = settings.AnimationPrefix + animName;
			}
			clip->SetName(animName);
			clip->SetSourcePath(sourcePath);
			
			// Duration
			float ticksPerSecond = anim->mTicksPerSecond > 0 ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;
			clip->SetTicksPerSecond(ticksPerSecond);
			clip->SetDuration(static_cast<float>(anim->mDuration) / ticksPerSecond);
			
			// Process channels
			for (unsigned int c = 0; c < anim->mNumChannels; c++) {
				aiNodeAnim* nodeAnim = anim->mChannels[c];
				
				AnimationChannel channel;
				channel.JointName = nodeAnim->mNodeName.C_Str();
				
				// Get all keyframe times
				std::set<float> keyframeTimes;
				for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; k++) {
					keyframeTimes.insert(static_cast<float>(nodeAnim->mPositionKeys[k].mTime) / ticksPerSecond);
				}
				for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; k++) {
					keyframeTimes.insert(static_cast<float>(nodeAnim->mRotationKeys[k].mTime) / ticksPerSecond);
				}
				for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; k++) {
					keyframeTimes.insert(static_cast<float>(nodeAnim->mScalingKeys[k].mTime) / ticksPerSecond);
				}
				
				// Create keyframes
				for (float time : keyframeTimes) {
					AnimationKeyframe kf;
					kf.Time = time;
					
					// Find position
					for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; k++) {
						float keyTime = static_cast<float>(nodeAnim->mPositionKeys[k].mTime) / ticksPerSecond;
						if (keyTime >= time) {
							kf.Translation = AssimpToGlmVec3(nodeAnim->mPositionKeys[k].mValue) * settings.Scale;
							break;
						}
					}
					
					// Find rotation
					for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; k++) {
						float keyTime = static_cast<float>(nodeAnim->mRotationKeys[k].mTime) / ticksPerSecond;
						if (keyTime >= time) {
							kf.Rotation = AssimpToGlmQuat(nodeAnim->mRotationKeys[k].mValue);
							break;
						}
					}
					
					// Find scale
					for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; k++) {
						float keyTime = static_cast<float>(nodeAnim->mScalingKeys[k].mTime) / ticksPerSecond;
						if (keyTime >= time) {
							kf.Scale = AssimpToGlmVec3(nodeAnim->mScalingKeys[k].mValue);
							break;
						}
					}
					
					channel.Keyframes.push_back(kf);
				}
				
				channel.SortKeyframes();
				clip->AddChannel(channel);
			}
			
			// Resolve joint indices if skeleton is available
			if (skeleton) {
				clip->ResolveJointIndices(*skeleton);
			}
			
			clip->ComputeDuration();
			clip->SetLoaded(true);
			
			clips.push_back(clip);
			
			LNX_LOG_INFO("Animation imported: {0} ({1} channels, {2:.2f}s)", 
				clip->GetName(), clip->GetChannelCount(), clip->GetDuration());
		}
		
		return clips;
	}
	
	Ref<AnimationClipAsset> AnimationImporter::ImportAnimation(
		const std::filesystem::path& sourcePath,
		const std::string& animationName,
		Ref<SkeletonAsset> skeleton,
		const AnimationImportSettings& settings)
	{
		auto clips = ImportAnimations(sourcePath, skeleton, settings);
		
		for (auto& clip : clips) {
			if (clip->GetName() == animationName) {
				return clip;
			}
		}
		
		LNX_LOG_WARN("AnimationImporter: Animation '{0}' not found in: {1}", animationName, sourcePath.string());
		return nullptr;
	}
	
	std::vector<AnimationImportResult> AnimationImporter::ImportBatch(
		const std::vector<std::filesystem::path>& sourcePaths,
		const std::filesystem::path& outputDir,
		const AnimationImportSettings& settings,
		ProgressCallback progressCallback)
	{
		std::vector<AnimationImportResult> results;
		results.reserve(sourcePaths.size());
		
		for (size_t i = 0; i < sourcePaths.size(); i++) {
			if (progressCallback) {
				progressCallback(sourcePaths[i].filename().string(),
					static_cast<int>(i + 1),
					static_cast<int>(sourcePaths.size()));
			}
			
			results.push_back(Import(sourcePaths[i], outputDir, settings));
		}
		
		return results;
	}
	
	AnimationImporter::ModelAnimationInfo AnimationImporter::GetAnimationInfo(const std::filesystem::path& sourcePath) {
		ModelAnimationInfo info;
		
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), 0);
		
		if (!scene) {
			return info;
		}
		
		// Check for bones
		for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
			if (scene->mMeshes[m]->mNumBones > 0) {
				info.HasSkeleton = true;
				info.BoneCount = std::max(info.BoneCount, scene->mMeshes[m]->mNumBones);
			}
		}
		
		// Get animation info
		for (unsigned int a = 0; a < scene->mNumAnimations; a++) {
			aiAnimation* anim = scene->mAnimations[a];
			
			AnimationInfo animInfo;
			animInfo.Name = anim->mName.length > 0 ? anim->mName.C_Str() : "Animation_" + std::to_string(a);
			animInfo.TicksPerSecond = anim->mTicksPerSecond > 0 ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;
			animInfo.Duration = static_cast<float>(anim->mDuration) / animInfo.TicksPerSecond;
			animInfo.ChannelCount = anim->mNumChannels;
			
			info.Animations.push_back(animInfo);
		}
		
		return info;
	}
	
	bool AnimationImporter::Validate(const std::filesystem::path& sourcePath, std::string& outError) {
		if (!std::filesystem::exists(sourcePath)) {
			outError = "File not found";
			return false;
		}
		
		if (!IsSupported(sourcePath)) {
			outError = "Unsupported file format";
			return false;
		}
		
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath.string(), 0);
		
		if (!scene) {
			outError = importer.GetErrorString();
			return false;
		}
		
		return true;
	}
	
	std::filesystem::path AnimationImporter::GenerateSkeletonPath(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const std::string& customName)
	{
		std::string name = customName.empty() ? sourcePath.stem().string() : customName;
		return outputDir / (name + ".luskel");
	}
	
	std::filesystem::path AnimationImporter::GenerateAnimationPath(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& outputDir,
		const std::string& animationName,
		const std::string& prefix)
	{
		std::string name = prefix + animationName;
		// Sanitize name
		std::replace(name.begin(), name.end(), ' ', '_');
		std::replace(name.begin(), name.end(), '|', '_');
		return outputDir / (name + ".luanim");
	}

}
