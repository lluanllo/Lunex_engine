#pragma once

/**
 * @file AnimationSystem.h
 * @brief Core animation system for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * Responsibilities:
 *   - Advance animation time
 *   - Sample keyframes from animation clips
 *   - Build skeleton hierarchy transforms
 *   - Apply animation blending
 *   - Generate final bone matrices for GPU skinning
 */

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Components/AnimationComponents.h"

#include <glm/glm.hpp>
#include <vector>
#include <entt.hpp>

namespace Lunex {

	// Forward declarations
	class SkeletonAsset;
	class AnimationClipAsset;

	/**
	 * @class AnimationSystem
	 * @brief Handles all skeletal animation for the scene
	 * 
	 * Update flow per frame:
	 *   1. Advance animation time for each AnimatorComponent
	 *   2. Sample current pose from animation clip
	 *   3. Sample next pose if blending
	 *   4. Blend poses if necessary
	 *   5. Build bone hierarchy (local to model space)
	 *   6. Apply inverse bind pose
	 *   7. Upload final matrices to GPU
	 */
	class AnimationSystem : public ISceneSystem {
	public:
		AnimationSystem();
		~AnimationSystem() override;

		// ========== ISceneSystem INTERFACE ==========
		
		void OnAttach(SceneContext& context) override;
		void OnDetach() override;
		
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		
		void OnUpdate(Timestep ts, SceneMode mode) override;
		void OnLateUpdate(Timestep ts) override;
		
		const std::string& GetName() const override { return m_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Animation; }
		
		bool IsActiveInMode(SceneMode mode) const override {
			// Animation runs in all modes (for previewing in editor)
			return true;
		}
		
		// ========== ANIMATION API ==========
		
		/**
		 * @brief Sample an animation clip at a specific time
		 * @param clip The animation clip to sample
		 * @param time Time in seconds
		 * @param outPose Output pose (one BoneTransform per joint)
		 */
		void SampleClip(
			const AnimationClipAsset& clip,
			float time,
			AnimationPose& outPose
		) const;
		
		/**
		 * @brief Blend two poses together
		 * @param poseA First pose
		 * @param poseB Second pose
		 * @param factor Blend factor (0 = poseA, 1 = poseB)
		 * @param outPose Output blended pose
		 */
		void BlendPoses(
			const AnimationPose& poseA,
			const AnimationPose& poseB,
			float factor,
			AnimationPose& outPose
		) const;
		
		/**
		 * @brief Build final bone matrices from a pose
		 * @param skeleton The skeleton definition
		 * @param pose Local bone transforms
		 * @param outMatrices Output model-space bone matrices
		 */
		void BuildBoneMatrices(
			const SkeletonAsset& skeleton,
			const AnimationPose& pose,
			std::vector<glm::mat4>& outMatrices
		) const;
		
	private:
		/**
		 * @brief Update a single entity's animation
		 */
		void UpdateEntityAnimation(
			AnimatorComponent& animator,
			SkeletalMeshComponent& skeletal,
			float deltaTime
		);
		
		/**
		 * @brief Advance animation time and handle looping
		 */
		void AdvanceTime(AnimatorComponent& animator, float deltaTime);
		
		/**
		 * @brief Handle animation blending transition
		 */
		void UpdateBlending(AnimatorComponent& animator, float deltaTime);
		
		/**
		 * @brief Process animation queue
		 */
		void ProcessQueue(AnimatorComponent& animator);
		
		/**
		 * @brief Upload bone matrices to GPU
		 */
		void UploadToGPU(SkeletalMeshComponent& skeletal);

	private:
		std::string m_Name = "AnimationSystem";
		
		// Temporary buffers (reused to avoid allocations)
		AnimationPose m_TempPoseA;
		AnimationPose m_TempPoseB;
		AnimationPose m_TempBlendedPose;
		std::vector<glm::mat4> m_TempModelSpaceMatrices;
	};

}
