#include "stpch.h"
#include "AnimationSystem.h"

#include "Scene/Components.h"
#include "Resources/Mesh/Model.h"
#include "Resources/Animation/Animation.h"
#include "Resources/Animation/IKSolver.h"
#include "Assets/Animation/AnimationAsset.h"
#include "Project/Project.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

namespace Lunex {

	void AnimationSystem::OnRuntimeStart(SceneMode mode) {
		if (!m_Context || !m_Context->Registry) return;
		auto view = m_Context->Registry->view<AnimatorComponent>();
		for (auto e : view) {
			auto& animator = view.get<AnimatorComponent>(e);
			animator.Time = 0.0f;
			animator.Playing = animator.PlayOnStart;
		}
	}

	void AnimationSystem::OnRuntimeStop() {
		if (!m_Context || !m_Context->Registry) return;
		auto view = m_Context->Registry->view<AnimatorComponent>();
		for (auto e : view) {
			auto& animator = view.get<AnimatorComponent>(e);
			animator.Playing = false;
			animator.Time = 0.0f;
		}
	}

	void AnimationSystem::OnUpdate(Timestep ts, SceneMode mode) {
		if (!m_Context || !m_Context->Registry) return;

		const bool runtime = (mode == SceneMode::Play || mode == SceneMode::Simulate);
		auto view = m_Context->Registry->view<MeshComponent, AnimatorComponent>();

		for (auto e : view) {
			auto& mesh = view.get<MeshComponent>(e);
			auto& animator = view.get<AnimatorComponent>(e);

			if (!mesh.MeshModel || !mesh.MeshModel->IsSkinned()) {
				animator.BoneMatrices.clear();
				continue;
			}

			const auto& clips = mesh.MeshModel->GetAnimations();
			const Skeleton& skel = *mesh.MeshModel->GetSkeleton();

			// Combined clip list: model clips first, then authored .luanim assets
			auto resolveClip = [&](int index) -> const AnimationClip* {
				if (index < 0) return nullptr;
				if (index < static_cast<int>(clips.size())) return clips[index].get();
				int assetIndex = index - static_cast<int>(clips.size());
				if (assetIndex >= static_cast<int>(animator.AnimationAssets.size())) return nullptr;
				std::filesystem::path p = animator.AnimationAssets[assetIndex];
				if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
				auto asset = AnimationAsset::GetOrLoad(p);
				return asset ? asset->GetClip(skel) : nullptr;
			};
			const int clipCount = static_cast<int>(clips.size() + animator.AnimationAssets.size());
			if (clipCount > 0) animator.ClipIndex = glm::clamp(animator.ClipIndex, 0, clipCount - 1);
			const AnimationClip* clip = resolveClip(animator.ClipIndex);

			const bool advance = runtime ? animator.Playing : animator.PreviewInEditor;
			if (advance && clip) {
				animator.Time += static_cast<float>(ts) * animator.Speed;
				float duration = clip->GetDurationSeconds();
				if (!animator.Loop && duration > 0.0f && animator.Time >= duration) {
					animator.Time = duration;
					if (runtime) animator.Playing = false;
				}
			}

			const Skeleton& skeleton = skel;
			std::vector<NodeTRS> pose;
			const bool editing = animator.UseEditPose && animator.EditPose.size() == skeleton.Nodes.size();
			if (editing) pose = animator.EditPose;
			else AnimationSampler::SampleLocalPose(skeleton, clip, animator.Time, animator.Loop, pose);

			// Crossfade from the previous clip
			if (!editing && animator.PreviousClip >= 0 && resolveClip(animator.PreviousClip) &&
				animator.BlendDuration > 0.0f && animator.BlendElapsed < animator.BlendDuration)
			{
				if (advance) {
					animator.BlendElapsed += static_cast<float>(ts);
					animator.PreviousTime += static_cast<float>(ts) * animator.Speed;
				}
				std::vector<NodeTRS> previous;
				AnimationSampler::SampleLocalPose(skeleton, resolveClip(animator.PreviousClip), animator.PreviousTime, true, previous);
				float w = animator.BlendElapsed / animator.BlendDuration;
				w = w * w * (3.0f - 2.0f * w); // smoothstep
				AnimationSampler::BlendPoses(previous, pose, w, pose);
			}
			else {
				animator.PreviousClip = -1;
			}

			// Runtime IK (applied on top of the animation; skipped while editing a pose)
			if (!editing && !animator.IKChains.empty() && m_Context->OwningScene) {
				Scene* scene = m_Context->OwningScene;
				glm::mat4 world = scene->GetWorldTransform(Entity{ e, scene });
				glm::mat4 invWorld = glm::inverse(world);
				for (const auto& chain : animator.IKChains) {
					if (!chain.Enabled || chain.TargetEntity == 0 || chain.Weight <= 0.0f) continue;
					auto node = skeleton.NodeIndexByName.find(chain.EndBone);
					if (node == skeleton.NodeIndexByName.end()) continue;
					Entity target = scene->GetEntityByUUID(UUID(chain.TargetEntity));
					if (!target) continue;
					glm::vec3 targetModel = glm::vec3(invWorld * scene->GetWorldTransform(target)[3]);
					IK::SolveCCD(skeleton, pose, node->second, chain.ChainLength, targetModel, 12, 1e-3f, chain.Weight);
				}
			}

			AnimationSampler::ComputePalette(skeleton, pose, animator.BoneMatrices,
				animator.ShowSkeleton ? &animator.NodeTransforms : nullptr);
		}
	}

} // namespace Lunex
