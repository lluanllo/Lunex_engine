#include "stpch.h"
#include "IKSolver.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Lunex::IK {

	static glm::mat4 ToMatrix(const NodeTRS& n) {
		return glm::translate(glm::mat4(1.0f), n.Translation) * glm::toMat4(n.Rotation) * glm::scale(glm::mat4(1.0f), n.Scale);
	}

	static glm::quat RotationOf(const glm::mat4& m) {
		glm::mat3 r(glm::normalize(glm::vec3(m[0])), glm::normalize(glm::vec3(m[1])), glm::normalize(glm::vec3(m[2])));
		return glm::normalize(glm::quat_cast(r));
	}

	void ComputeModelSpace(const Skeleton& skeleton, const std::vector<NodeTRS>& local, std::vector<glm::mat4>& outModel) {
		const size_t n = std::min(skeleton.Nodes.size(), local.size());
		std::vector<glm::mat4> global(n);
		outModel.resize(n);
		for (size_t i = 0; i < n; i++) {
			int parent = skeleton.Nodes[i].Parent;
			global[i] = (parent >= 0) ? global[parent] * ToMatrix(local[i]) : ToMatrix(local[i]);
			outModel[i] = skeleton.GlobalInverse * global[i];
		}
	}

	// Parent's model-space transform (for the root: the inverse of GlobalInverse)
	static glm::mat4 ParentModel(const Skeleton& skeleton, const std::vector<glm::mat4>& model, int node) {
		int parent = skeleton.Nodes[node].Parent;
		return parent >= 0 ? model[parent] : skeleton.GlobalInverse;
	}

	void SetNodeModelTransform(const Skeleton& skeleton, std::vector<NodeTRS>& local,
		const std::vector<glm::mat4>& model, int node, const glm::mat4& desiredModel,
		bool applyRotation, bool applyPosition)
	{
		if (node < 0 || node >= static_cast<int>(local.size())) return;
		glm::mat4 newLocal = glm::inverse(ParentModel(skeleton, model, node)) * desiredModel;
		if (applyPosition) local[node].Translation = glm::vec3(newLocal[3]);
		if (applyRotation) local[node].Rotation = RotationOf(newLocal);
	}

	bool SolveCCD(const Skeleton& skeleton, std::vector<NodeTRS>& local, int endNode, int chainLength,
		const glm::vec3& target, int iterations, float tolerance, float weight)
	{
		if (endNode < 0 || endNode >= static_cast<int>(skeleton.Nodes.size()) || chainLength < 1) return false;

		// Joints from the end effector's parent upwards
		std::vector<int> chain;
		for (int j = skeleton.Nodes[endNode].Parent; j >= 0 && static_cast<int>(chain.size()) < chainLength; j = skeleton.Nodes[j].Parent)
			chain.push_back(j);
		if (chain.empty()) return false;

		const std::vector<NodeTRS> original = local;
		std::vector<glm::mat4> model;
		bool reached = false;

		for (int it = 0; it < iterations && !reached; it++) {
			for (int joint : chain) {
				ComputeModelSpace(skeleton, local, model);
				glm::vec3 jointPos = glm::vec3(model[joint][3]);
				glm::vec3 effector = glm::vec3(model[endNode][3]);

				glm::vec3 toEffector = effector - jointPos;
				glm::vec3 toTarget = target - jointPos;
				if (glm::length(toEffector) < 1e-6f || glm::length(toTarget) < 1e-6f) continue;

				// Model-space rotation that swings the effector towards the target
				glm::quat delta = glm::rotation(glm::normalize(toEffector), glm::normalize(toTarget));

				// Convert to the joint's local frame: L' = P^-1 * D * P * L
				glm::quat parentRot = RotationOf(ParentModel(skeleton, model, joint));
				local[joint].Rotation = glm::normalize(glm::inverse(parentRot) * delta * parentRot * local[joint].Rotation);
			}
			ComputeModelSpace(skeleton, local, model);
			reached = glm::length(glm::vec3(model[endNode][3]) - target) <= tolerance;
		}

		if (weight < 1.0f) {
			for (int joint : chain) {
				local[joint].Rotation = glm::normalize(glm::slerp(original[joint].Rotation, local[joint].Rotation, glm::clamp(weight, 0.0f, 1.0f)));
			}
		}
		return reached;
	}

} // namespace Lunex::IK
