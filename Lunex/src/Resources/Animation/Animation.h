#pragma once

/**
 * @file Animation.h
 * @brief Skeletal animation data: skeleton hierarchy, animation clips and pose sampling.
 *
 * Data flow:
 *   Model (assimp import) -> Skeleton + AnimationClips (shared, immutable)
 *   AnimatorComponent (per entity) -> time / clip state + final bone matrices
 *   Renderer -> uploads bone matrices to an SSBO, vertex shaders do linear blend skinning
 */

#include "Core/Core.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace Lunex {

	// Maximum bones influencing a single vertex (matches ivec4/vec4 vertex attributes)
	constexpr int MAX_BONE_INFLUENCES = 4;
	// Upper bound on bones per skeleton (SSBO is sized per draw, this is a sanity limit)
	constexpr int MAX_BONES = 512;

	/**
	 * @struct SkeletonNode
	 * @brief One node of the imported scene hierarchy (bones and their non-bone parents)
	 *
	 * Nodes are stored parents-first, so a single forward pass computes global transforms.
	 */
	struct SkeletonNode {
		std::string Name;
		int Parent = -1;                       // index into Skeleton::Nodes, -1 for root
		glm::mat4 LocalBindTransform{ 1.0f };  // node transform in the bind pose
		int BoneIndex = -1;                    // index into bone palette, -1 if not a bone
	};

	/**
	 * @struct Skeleton
	 */
	struct Skeleton {
		std::vector<SkeletonNode> Nodes;
		std::vector<glm::mat4> BoneOffsets;               // mesh space -> bone space (inverse bind)
		std::unordered_map<std::string, int> BoneIndexByName;
		std::unordered_map<std::string, int> NodeIndexByName;
		glm::mat4 GlobalInverse{ 1.0f };                  // inverse of the root transform

		int GetBoneCount() const { return static_cast<int>(BoneOffsets.size()); }

		// Register a bone (returns existing index if already present)
		int AddBone(const std::string& name, const glm::mat4& offset);
	};

	template<typename T>
	struct Keyframe {
		float Time = 0.0f;   // in ticks
		T Value{};
	};

	/**
	 * @struct AnimationChannel
	 * @brief Keyframes for one node
	 */
	struct AnimationChannel {
		int NodeIndex = -1;
		std::vector<Keyframe<glm::vec3>> Positions;
		std::vector<Keyframe<glm::quat>> Rotations;
		std::vector<Keyframe<glm::vec3>> Scales;
	};

	/**
	 * @struct AnimationClip
	 */
	struct AnimationClip {
		std::string Name;
		float DurationTicks = 0.0f;
		float TicksPerSecond = 25.0f;
		std::vector<AnimationChannel> Channels;

		float GetDurationSeconds() const {
			return TicksPerSecond > 0.0f ? DurationTicks / TicksPerSecond : 0.0f;
		}
	};

	/**
	 * @namespace AnimationSampler
	 */
	// Local (parent-relative) transform of one skeleton node
	struct NodeTRS {
		glm::vec3 Translation{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale{ 1.0f };
	};

	namespace AnimationSampler {

		// Local transform of every node for a clip at a time (bind pose for null clip)
		void SampleLocalPose(const Skeleton& skeleton, const AnimationClip* clip, float timeSeconds, bool loop,
			std::vector<NodeTRS>& outLocal);

		// out = lerp/slerp(a, b, weight)  (weight 0 = a, 1 = b)
		void BlendPoses(const std::vector<NodeTRS>& a, const std::vector<NodeTRS>& b, float weight,
			std::vector<NodeTRS>& out);

		// Skinning palette from local poses. Optionally returns each node's model-space transform
		// (GlobalInverse applied) for skeleton visualization.
		void ComputePalette(const Skeleton& skeleton, const std::vector<NodeTRS>& local,
			std::vector<glm::mat4>& outBoneMatrices, std::vector<glm::mat4>* outNodeModelSpace = nullptr);

		/**
		 * @brief Compute the skinning palette for a clip at a given time.
		 * @param clip  may be null -> bind pose
		 * @param timeSeconds playback time (wrapped when loop is true, clamped otherwise)
		 * @param outBoneMatrices resized to the skeleton's bone count
		 */
		void SamplePose(const Skeleton& skeleton, const AnimationClip* clip, float timeSeconds, bool loop,
			std::vector<glm::mat4>& outBoneMatrices);

	} // namespace AnimationSampler

} // namespace Lunex
