#include "stpch.h"
#include "AnimationSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Assets/Animation/SkeletonAsset.h"
#include "Assets/Animation/AnimationClipAsset.h"
#include "Renderer/StorageBuffer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <entt.hpp>

namespace Lunex {

	// ============================================================================
	// CONSTRUCTOR / DESTRUCTOR
	// ============================================================================
	
	AnimationSystem::AnimationSystem() {
	}
	
	AnimationSystem::~AnimationSystem() {
	}

	// ============================================================================
	// LIFECYCLE
	// ============================================================================
	
	void AnimationSystem::OnAttach(SceneContext& context) {
		m_Context = &context;
		LNX_LOG_INFO("AnimationSystem attached");
	}
	
	void AnimationSystem::OnDetach() {
		m_Context = nullptr;
		LNX_LOG_INFO("AnimationSystem detached");
	}
	
	void AnimationSystem::OnRuntimeStart(SceneMode mode) {
		LNX_LOG_INFO("AnimationSystem: Runtime started (mode: {0})", 
			mode == SceneMode::Play ? "Play" : "Simulate");
		
		// Reset all animations to start
		if (m_Context && m_Context->Registry) {
			auto view = m_Context->Registry->view<AnimatorComponent>();
			for (auto entity : view) {
				auto& animator = view.get<AnimatorComponent>(entity);
				animator.CurrentTime = 0.0f;
				if (animator.HasAnimation()) {
					animator.IsPlaying = true;
				}
			}
		}
	}
	
	void AnimationSystem::OnRuntimeStop() {
		LNX_LOG_INFO("AnimationSystem: Runtime stopped");
		
		// Stop all animations and reset to bind pose
		if (m_Context && m_Context->Registry) {
			auto view = m_Context->Registry->view<AnimatorComponent, SkeletalMeshComponent>();
			for (auto entity : view) {
				auto& animator = view.get<AnimatorComponent>(entity);
				auto& skeletal = view.get<SkeletalMeshComponent>(entity);
				
				animator.IsPlaying = false;
				animator.CurrentTime = 0.0f;
				skeletal.ResetToBindPose();
			}
		}
	}

	// ============================================================================
	// UPDATE
	// ============================================================================
	
	void AnimationSystem::OnUpdate(Timestep ts, SceneMode mode) {
		if (!m_Context || !m_Context->Registry) return;
		
		float deltaTime = ts.GetSeconds();
		
		// Update all entities with both AnimatorComponent and SkeletalMeshComponent
		auto view = m_Context->Registry->view<AnimatorComponent, SkeletalMeshComponent>();
		
		for (auto entity : view) {
			auto& animator = view.get<AnimatorComponent>(entity);
			auto& skeletal = view.get<SkeletalMeshComponent>(entity);
			
			if (!skeletal.IsValid()) continue;
			
			UpdateEntityAnimation(animator, skeletal, deltaTime);
		}
	}
	
	void AnimationSystem::OnLateUpdate(Timestep ts) {
		// Upload bone matrices to GPU after all updates
		if (!m_Context || !m_Context->Registry) return;
		
		auto view = m_Context->Registry->view<SkeletalMeshComponent>();
		for (auto entity : view) {
			auto& skeletal = view.get<SkeletalMeshComponent>(entity);
			if (skeletal.BoneMatricesDirty) {
				UploadToGPU(skeletal);
			}
		}
	}

	// ============================================================================
	// ENTITY UPDATE
	// ============================================================================
	
	void AnimationSystem::UpdateEntityAnimation(
		AnimatorComponent& animator,
		SkeletalMeshComponent& skeletal,
		float deltaTime)
	{
		// Early out if no animation or not playing
		if (!animator.HasAnimation() || !animator.IsPlaying) {
			return;
		}
		
		// Advance time
		AdvanceTime(animator, deltaTime);
		
		// Update blending
		if (animator.IsBlending) {
			UpdateBlending(animator, deltaTime);
		}
		
		// Sample current animation
		uint32_t boneCount = skeletal.GetBoneCount();
		if (boneCount == 0) return;
		
		// Ensure temporary buffers are sized correctly
		m_TempPoseA.resize(boneCount);
		m_TempPoseB.resize(boneCount);
		m_TempBlendedPose.resize(boneCount);
		m_TempModelSpaceMatrices.resize(boneCount);
		
		// Initialize poses to identity
		for (uint32_t i = 0; i < boneCount; i++) {
			m_TempPoseA[i] = BoneTransform();
			m_TempPoseB[i] = BoneTransform();
		}
		
		// Sample current clip
		if (animator.CurrentClip) {
			SampleClip(*animator.CurrentClip, animator.CurrentTime, m_TempPoseA);
		}
		
		// Handle blending
		if (animator.IsBlending && animator.NextClip) {
			SampleClip(*animator.NextClip, 0.0f, m_TempPoseB);
			BlendPoses(m_TempPoseA, m_TempPoseB, animator.GetBlendFactor(), m_TempBlendedPose);
		}
		else {
			m_TempBlendedPose = m_TempPoseA;
		}
		
		// Build final bone matrices
		BuildBoneMatrices(*skeletal.Skeleton, m_TempBlendedPose, m_TempModelSpaceMatrices);
		
		// Apply inverse bind pose and store
		skeletal.BoneMatrices.resize(boneCount);
		auto inverseBindPoses = skeletal.Skeleton->GetInverseBindPoseMatrices();
		
		for (uint32_t i = 0; i < boneCount; i++) {
			skeletal.BoneMatrices[i] = m_TempModelSpaceMatrices[i] * inverseBindPoses[i];
		}
		
		skeletal.BoneMatricesDirty = true;
	}
	
	void AnimationSystem::AdvanceTime(AnimatorComponent& animator, float deltaTime) {
		animator.CurrentTime += deltaTime * animator.PlaybackSpeed;
		
		float duration = animator.GetDuration();
		if (duration <= 0.0f) return;
		
		if (animator.Loop) {
			// Wrap around
			while (animator.CurrentTime >= duration) {
				animator.CurrentTime -= duration;
			}
			while (animator.CurrentTime < 0.0f) {
				animator.CurrentTime += duration;
			}
		}
		else {
			// Clamp and check for finish
			if (animator.CurrentTime >= duration) {
				animator.CurrentTime = duration;
				animator.IsPlaying = false;
				
				// Process queue if animation finished
				ProcessQueue(animator);
			}
		}
	}
	
	void AnimationSystem::UpdateBlending(AnimatorComponent& animator, float deltaTime) {
		animator.BlendTime += deltaTime;
		
		if (animator.BlendTime >= animator.BlendDuration) {
			// Blend complete - switch to next animation
			animator.CurrentClip = animator.NextClip;
			animator.CurrentClipID = animator.NextClipID;
			animator.CurrentTime = animator.BlendTime - animator.BlendDuration;
			
			animator.NextClip = nullptr;
			animator.NextClipID = UUID(0);
			animator.BlendTime = 0.0f;
			animator.IsBlending = false;
		}
	}
	
	void AnimationSystem::ProcessQueue(AnimatorComponent& animator) {
		if (animator.AnimationQueue.empty()) return;
		
		auto& queued = animator.AnimationQueue.front();
		animator.CrossFadeTo(queued.Clip, queued.BlendDuration, queued.Loop);
		animator.AnimationQueue.erase(animator.AnimationQueue.begin());
	}

	// ============================================================================
	// SAMPLING & BLENDING
	// ============================================================================
	
	void AnimationSystem::SampleClip(
		const AnimationClipAsset& clip,
		float time,
		AnimationPose& outPose) const
	{
		for (const auto& channel : clip.GetChannels()) {
			if (channel.JointIndex < 0 || channel.JointIndex >= static_cast<int32_t>(outPose.size())) {
				continue;
			}
			
			AnimationKeyframe kf = channel.Sample(time);
			
			outPose[channel.JointIndex].Translation = kf.Translation;
			outPose[channel.JointIndex].Rotation = kf.Rotation;
			outPose[channel.JointIndex].Scale = kf.Scale;
		}
	}
	
	void AnimationSystem::BlendPoses(
		const AnimationPose& poseA,
		const AnimationPose& poseB,
		float factor,
		AnimationPose& outPose) const
	{
		size_t count = std::min(poseA.size(), poseB.size());
		outPose.resize(count);
		
		for (size_t i = 0; i < count; i++) {
			outPose[i] = BoneTransform::Lerp(poseA[i], poseB[i], factor);
		}
	}
	
	void AnimationSystem::BuildBoneMatrices(
		const SkeletonAsset& skeleton,
		const AnimationPose& pose,
		std::vector<glm::mat4>& outMatrices) const
	{
		uint32_t boneCount = skeleton.GetJointCount();
		outMatrices.resize(boneCount);
		
		// Build in hierarchical order (parents before children)
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = skeleton.GetJoint(i);
			
			// Get local transform from pose (or identity if not animated)
			glm::mat4 localTransform;
			if (i < pose.size()) {
				localTransform = pose[i].ToMatrix();
			}
			else {
				localTransform = joint.GetLocalTransform();
			}
			
			// Multiply by parent's model-space transform
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(i)) {
				outMatrices[i] = outMatrices[joint.ParentIndex] * localTransform;
			}
			else {
				outMatrices[i] = localTransform;
			}
		}
	}

	// ============================================================================
	// GPU UPLOAD
	// ============================================================================
	
	void AnimationSystem::UploadToGPU(SkeletalMeshComponent& skeletal) {
		if (skeletal.BoneMatrices.empty()) return;
		
		// Create storage buffer if needed
		if (!skeletal.BoneMatrixBuffer) {
			uint32_t bufferSize = static_cast<uint32_t>(skeletal.BoneMatrices.size() * sizeof(glm::mat4));
			skeletal.BoneMatrixBuffer = StorageBuffer::Create(bufferSize, 10); // Binding 10 for bone matrices
		}
		
		// Upload data
		if (skeletal.BoneMatrixBuffer) {
			skeletal.BoneMatrixBuffer->SetData(
				skeletal.BoneMatrices.data(),
				static_cast<uint32_t>(skeletal.BoneMatrices.size() * sizeof(glm::mat4))
			);
		}
		
		skeletal.BoneMatricesDirty = false;
	}

}
