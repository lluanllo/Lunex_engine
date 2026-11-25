#include "stpch.h"
#include "Animation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Lunex {

	int Skeleton::AddBone(const std::string& name, const glm::mat4& offset) {
		auto it = BoneIndexByName.find(name);
		if (it != BoneIndexByName.end()) return it->second;
		int index = static_cast<int>(BoneOffsets.size());
		BoneOffsets.push_back(offset);
		BoneIndexByName[name] = index;
		return index;
	}

	namespace {

		// Index of the last key whose time <= t (keys sorted by time)
		template<typename T>
		size_t FindKey(const std::vector<Keyframe<T>>& keys, float t) {
			auto it = std::upper_bound(keys.begin(), keys.end(), t,
				[](float value, const Keyframe<T>& k) { return value < k.Time; });
			if (it == keys.begin()) return 0;
			return static_cast<size_t>(std::distance(keys.begin(), it) - 1);
		}

		template<typename T>
		float Factor(const std::vector<Keyframe<T>>& keys, size_t i, float t) {
			float t0 = keys[i].Time, t1 = keys[i + 1].Time;
			float span = t1 - t0;
			return span > 0.0f ? glm::clamp((t - t0) / span, 0.0f, 1.0f) : 0.0f;
		}

		glm::vec3 SampleVec3(const std::vector<Keyframe<glm::vec3>>& keys, float t, const glm::vec3& fallback) {
			if (keys.empty()) return fallback;
			if (keys.size() == 1) return keys[0].Value;
			size_t i = FindKey(keys, t);
			if (i + 1 >= keys.size()) return keys.back().Value;
			return glm::mix(keys[i].Value, keys[i + 1].Value, Factor(keys, i, t));
		}

		glm::quat SampleQuat(const std::vector<Keyframe<glm::quat>>& keys, float t, const glm::quat& fallback) {
			if (keys.empty()) return fallback;
			if (keys.size() == 1) return keys[0].Value;
			size_t i = FindKey(keys, t);
			if (i + 1 >= keys.size()) return keys.back().Value;
			return glm::normalize(glm::slerp(keys[i].Value, keys[i + 1].Value, Factor(keys, i, t)));
		}

		void Decompose(const glm::mat4& m, glm::vec3& t, glm::quat& r, glm::vec3& s) {
			t = glm::vec3(m[3]);
			s = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
			glm::mat3 rot(
				s.x != 0.0f ? glm::vec3(m[0]) / s.x : glm::vec3(1, 0, 0),
				s.y != 0.0f ? glm::vec3(m[1]) / s.y : glm::vec3(0, 1, 0),
				s.z != 0.0f ? glm::vec3(m[2]) / s.z : glm::vec3(0, 0, 1));
			r = glm::quat_cast(rot);
		}
	}

	namespace AnimationSampler {

		void SampleLocalPose(const Skeleton& skeleton, const AnimationClip* clip, float timeSeconds, bool loop,
			std::vector<NodeTRS>& outLocal)
		{
			const size_t nodeCount = skeleton.Nodes.size();
			outLocal.resize(nodeCount);
			for (size_t i = 0; i < nodeCount; i++) {
				Decompose(skeleton.Nodes[i].LocalBindTransform, outLocal[i].Translation, outLocal[i].Rotation, outLocal[i].Scale);
			}
			if (!clip || clip->DurationTicks <= 0.0f) return;

			float ticks = timeSeconds * clip->TicksPerSecond;
			ticks = loop ? std::fmod(std::max(ticks, 0.0f), clip->DurationTicks)
			             : glm::clamp(ticks, 0.0f, clip->DurationTicks);

			for (const auto& ch : clip->Channels) {
				if (ch.NodeIndex < 0 || ch.NodeIndex >= static_cast<int>(nodeCount)) continue;
				NodeTRS& n = outLocal[ch.NodeIndex];
				n.Translation = SampleVec3(ch.Positions, ticks, n.Translation);
				n.Rotation = SampleQuat(ch.Rotations, ticks, n.Rotation);
				n.Scale = SampleVec3(ch.Scales, ticks, n.Scale);
			}
		}

		void BlendPoses(const std::vector<NodeTRS>& a, const std::vector<NodeTRS>& b, float weight,
			std::vector<NodeTRS>& out)
		{
			const size_t n = std::min(a.size(), b.size());
			out.resize(n);
			weight = glm::clamp(weight, 0.0f, 1.0f);
			for (size_t i = 0; i < n; i++) {
				out[i].Translation = glm::mix(a[i].Translation, b[i].Translation, weight);
				out[i].Rotation = glm::normalize(glm::slerp(a[i].Rotation, b[i].Rotation, weight));
				out[i].Scale = glm::mix(a[i].Scale, b[i].Scale, weight);
			}
		}

		void ComputePalette(const Skeleton& skeleton, const std::vector<NodeTRS>& local,
			std::vector<glm::mat4>& outBoneMatrices, std::vector<glm::mat4>* outNodeModelSpace)
		{
			const size_t nodeCount = std::min(skeleton.Nodes.size(), local.size());
			outBoneMatrices.assign(skeleton.BoneOffsets.size(), glm::mat4(1.0f));
			if (outNodeModelSpace) outNodeModelSpace->resize(nodeCount);

			// Parents are stored before children: one pass gives global transforms
			std::vector<glm::mat4> global(nodeCount);
			for (size_t i = 0; i < nodeCount; i++) {
				const NodeTRS& n = local[i];
				glm::mat4 m = glm::translate(glm::mat4(1.0f), n.Translation) * glm::toMat4(n.Rotation) * glm::scale(glm::mat4(1.0f), n.Scale);
				int parent = skeleton.Nodes[i].Parent;
				global[i] = (parent >= 0) ? global[parent] * m : m;

				if (outNodeModelSpace) (*outNodeModelSpace)[i] = skeleton.GlobalInverse * global[i];

				int bone = skeleton.Nodes[i].BoneIndex;
				if (bone >= 0 && bone < static_cast<int>(outBoneMatrices.size())) {
					outBoneMatrices[bone] = skeleton.GlobalInverse * global[i] * skeleton.BoneOffsets[bone];
				}
			}
		}

		void SamplePose(const Skeleton& skeleton, const AnimationClip* clip, float timeSeconds, bool loop,
			std::vector<glm::mat4>& outBoneMatrices)
		{
			std::vector<NodeTRS> local;
			SampleLocalPose(skeleton, clip, timeSeconds, loop, local);
			ComputePalette(skeleton, local, outBoneMatrices);
		}

	} // namespace AnimationSampler

} // namespace Lunex
