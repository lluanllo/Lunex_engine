#pragma once

/**
 * @file AnimationComponents.h
 * @brief Components for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * Components:
 *   - SkeletalMeshComponent: Links mesh with skeleton for skinning
 *   - AnimatorComponent: Controls animation playback and blending
 */

#include "Core/UUID.h"
#include "Core/Core.h"
#include "Assets/Animation/SkeletonAsset.h"
#include "Assets/Animation/AnimationClipAsset.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Renderer/Buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace Lunex {

	// Forward declaration
	class StorageBuffer;

	// ============================================================================
	// BONE TRANSFORM (Runtime pose data)
	// ============================================================================
	
	struct BoneTransform {
		glm::vec3 Translation = glm::vec3(0.0f);
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 Scale = glm::vec3(1.0f);
		
		glm::mat4 ToMatrix() const;
		
		static BoneTransform Lerp(const BoneTransform& a, const BoneTransform& b, float t);
	};

	// ============================================================================
	// ANIMATION POSE (Full skeleton pose at a point in time)
	// ============================================================================
	
	using AnimationPose = std::vector<BoneTransform>;

	// ============================================================================
	// SKELETAL MESH COMPONENT
	// ============================================================================
	/**
	 * @struct SkeletalMeshComponent
	 * @brief Links a mesh with a skeleton for GPU skinning
	 * 
	 * This component is required for any entity that uses skeletal animation.
	 * It references:
	 *   - A MeshAsset (the visual mesh with bone weights)
	 *   - A SkeletonAsset (the bone hierarchy)
	 *   - Runtime bone matrices for GPU skinning
	 */
	struct SkeletalMeshComponent {
		// ========== ASSET REFERENCES ==========
		
		// The skeletal mesh (with bone weights)
		Ref<MeshAsset> Mesh;
		UUID MeshAssetID;
		std::string MeshAssetPath;
		
		// The skeleton definition
		Ref<SkeletonAsset> Skeleton;
		UUID SkeletonAssetID;
		std::string SkeletonAssetPath;
		
		// ========== RUNTIME DATA ==========
		
		// Final bone matrices (after animation, ready for GPU)
		std::vector<glm::mat4> BoneMatrices;
		
		// GPU buffer for bone matrices (storage buffer)
		Ref<StorageBuffer> BoneMatrixBuffer;
		
		// Dirty flag for GPU upload
		bool BoneMatricesDirty = true;
		
		// ========== CONSTRUCTORS ==========
		
		SkeletalMeshComponent() = default;
		
		SkeletalMeshComponent(const SkeletalMeshComponent& other)
			: Mesh(other.Mesh)
			, MeshAssetID(other.MeshAssetID)
			, MeshAssetPath(other.MeshAssetPath)
			, Skeleton(other.Skeleton)
			, SkeletonAssetID(other.SkeletonAssetID)
			, SkeletonAssetPath(other.SkeletonAssetPath)
			, BoneMatrices(other.BoneMatrices)
			, BoneMatricesDirty(true)
		{
		}
		
		// ========== MESH API ==========
		
		void SetMesh(Ref<MeshAsset> meshAsset) {
			Mesh = meshAsset;
			if (meshAsset) {
				MeshAssetID = meshAsset->GetID();
				MeshAssetPath = meshAsset->GetPath().string();
			}
		}
		
		void SetMesh(const std::filesystem::path& path) {
			Mesh = MeshAsset::LoadFromFile(path);
			if (Mesh) {
				MeshAssetID = Mesh->GetID();
				MeshAssetPath = path.string();
			}
		}
		
		// ========== SKELETON API ==========
		
		void SetSkeleton(Ref<SkeletonAsset> skeleton) {
			Skeleton = skeleton;
			if (skeleton) {
				SkeletonAssetID = skeleton->GetID();
				SkeletonAssetPath = skeleton->GetPath().string();
				
				// Initialize bone matrices
				uint32_t boneCount = skeleton->GetJointCount();
				BoneMatrices.resize(boneCount, glm::mat4(1.0f));
				BoneMatricesDirty = true;
			}
		}
		
		void SetSkeleton(const std::filesystem::path& path) {
			Skeleton = SkeletonAsset::LoadFromFile(path);
			SetSkeleton(Skeleton);
		}
		
		// ========== BONE MATRICES ==========
		
		void SetBoneMatrices(const std::vector<glm::mat4>& matrices) {
			BoneMatrices = matrices;
			BoneMatricesDirty = true;
		}
		
		void SetBoneMatrix(uint32_t index, const glm::mat4& matrix) {
			if (index < BoneMatrices.size()) {
				BoneMatrices[index] = matrix;
				BoneMatricesDirty = true;
			}
		}
		
		uint32_t GetBoneCount() const {
			return Skeleton ? Skeleton->GetJointCount() : 0;
		}
		
		// Reset to bind pose
		void ResetToBindPose() {
			if (Skeleton) {
				for (uint32_t i = 0; i < Skeleton->GetJointCount(); i++) {
					BoneMatrices[i] = glm::mat4(1.0f);
				}
				BoneMatricesDirty = true;
			}
		}
		
		// ========== VALIDATION ==========
		
		bool IsValid() const {
			return Mesh != nullptr && Skeleton != nullptr;
		}
		
		bool HasSkeleton() const {
			return Skeleton != nullptr;
		}
	};

	// ============================================================================
	// ANIMATOR COMPONENT
	// ============================================================================
	/**
	 * @struct AnimatorComponent
	 * @brief Controls animation playback for a skeletal mesh
	 * 
	 * Features:
	 *   - Single animation playback
	 *   - Animation blending (crossfade)
	 *   - Playback speed control
	 *   - Loop/one-shot modes
	 */
	struct AnimatorComponent {
		// ========== CURRENT ANIMATION ==========
		
		Ref<AnimationClipAsset> CurrentClip;
		UUID CurrentClipID;
		std::string CurrentClipPath;
		
		float CurrentTime = 0.0f;           // Current playback time in seconds
		float PlaybackSpeed = 1.0f;         // Playback speed multiplier
		bool IsPlaying = false;             // Is animation playing?
		bool Loop = true;                   // Loop when finished?
		
		// ========== BLENDING ==========
		
		Ref<AnimationClipAsset> NextClip;   // Clip to blend to
		UUID NextClipID;
		float BlendTime = 0.0f;             // Current blend progress
		float BlendDuration = 0.2f;         // Total blend duration
		bool IsBlending = false;            // Is currently blending?
		
		// ========== ANIMATION QUEUE ==========
		
		struct QueuedAnimation {
			Ref<AnimationClipAsset> Clip;
			float BlendDuration = 0.2f;
			bool Loop = true;
		};
		std::vector<QueuedAnimation> AnimationQueue;
		
		// ========== CONSTRUCTORS ==========
		
		AnimatorComponent() = default;
		
		AnimatorComponent(const AnimatorComponent& other)
			: CurrentClip(other.CurrentClip)
			, CurrentClipID(other.CurrentClipID)
			, CurrentClipPath(other.CurrentClipPath)
			, CurrentTime(0.0f)
			, PlaybackSpeed(other.PlaybackSpeed)
			, IsPlaying(false)
			, Loop(other.Loop)
			, BlendDuration(other.BlendDuration)
		{
		}
		
		// ========== PLAYBACK CONTROL ==========
		
		void Play(Ref<AnimationClipAsset> clip, bool loop = true) {
			CurrentClip = clip;
			if (clip) {
				CurrentClipID = clip->GetID();
				CurrentClipPath = clip->GetPath().string();
			}
			CurrentTime = 0.0f;
			IsPlaying = true;
			Loop = loop;
			IsBlending = false;
		}
		
		void Play(const std::filesystem::path& clipPath, bool loop = true) {
			auto clip = AnimationClipAsset::LoadFromFile(clipPath);
			Play(clip, loop);
		}
		
		void Stop() {
			IsPlaying = false;
			CurrentTime = 0.0f;
		}
		
		void Pause() {
			IsPlaying = false;
		}
		
		void Resume() {
			IsPlaying = true;
		}
		
		// ========== BLENDING ==========
		
		void CrossFadeTo(Ref<AnimationClipAsset> clip, float duration = 0.2f, bool loop = true) {
			if (!CurrentClip) {
				Play(clip, loop);
				return;
			}
			
			NextClip = clip;
			if (clip) {
				NextClipID = clip->GetID();
			}
			BlendTime = 0.0f;
			BlendDuration = duration;
			IsBlending = true;
			Loop = loop;
		}
		
		void QueueAnimation(Ref<AnimationClipAsset> clip, float blendDuration = 0.2f, bool loop = true) {
			QueuedAnimation qa;
			qa.Clip = clip;
			qa.BlendDuration = blendDuration;
			qa.Loop = loop;
			AnimationQueue.push_back(qa);
		}
		
		// ========== STATE QUERIES ==========
		
		float GetNormalizedTime() const {
			if (!CurrentClip || CurrentClip->GetDuration() <= 0.0f) return 0.0f;
			return CurrentTime / CurrentClip->GetDuration();
		}
		
		float GetDuration() const {
			return CurrentClip ? CurrentClip->GetDuration() : 0.0f;
		}
		
		bool IsFinished() const {
			if (!CurrentClip || Loop) return false;
			return CurrentTime >= CurrentClip->GetDuration();
		}
		
		float GetBlendFactor() const {
			if (!IsBlending || BlendDuration <= 0.0f) return 0.0f;
			return glm::clamp(BlendTime / BlendDuration, 0.0f, 1.0f);
		}
		
		// ========== VALIDATION ==========
		
		bool HasAnimation() const {
			return CurrentClip != nullptr;
		}
	};

}
