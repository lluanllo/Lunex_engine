#pragma once

/**
 * @file IKSolver.h
 * @brief Inverse kinematics on skeleton poses (FK is the plain local-pose evaluation).
 *
 * CCD (cyclic coordinate descent) works for any chain length (arms, legs, tails,
 * spines, tentacles), so it accepts every kind of rig. All positions are in model
 * space (the space AnimationSampler::ComputePalette reports node transforms in).
 */

#include "Animation.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lunex::IK {

	// Model-space transform of every node for a local pose
	void ComputeModelSpace(const Skeleton& skeleton, const std::vector<NodeTRS>& local,
		std::vector<glm::mat4>& outModel);

	/**
	 * @brief Rotate the chain [endNode's ancestors up to chainLength joints] so endNode reaches target.
	 * @param weight 0..1 blend between the input pose and the solved pose
	 * @return true if the end effector is within tolerance of the target
	 */
	bool SolveCCD(const Skeleton& skeleton, std::vector<NodeTRS>& local, int endNode, int chainLength,
		const glm::vec3& targetModelSpace, int iterations = 12, float tolerance = 1e-3f, float weight = 1.0f);

	// Set a node's local transform from a desired model-space transform (FK editing helper)
	void SetNodeModelTransform(const Skeleton& skeleton, std::vector<NodeTRS>& local,
		const std::vector<glm::mat4>& model, int node, const glm::mat4& desiredModel,
		bool applyRotation, bool applyPosition);

} // namespace Lunex::IK
