#pragma once

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

namespace Lunex {

	/**
	 * @class AnimationSystem
	 * @brief Advances AnimatorComponents and computes their skinning palettes.
	 *
	 * Runs after scripts and physics so gameplay can change the clip/speed in the
	 * same frame. In Edit mode only animators with PreviewInEditor advance; the
	 * rest show the pose at their current time (bind pose at t = 0).
	 */
	class AnimationSystem : public ISceneSystem {
	public:
		AnimationSystem() = default;
		~AnimationSystem() override = default;

		void OnAttach(SceneContext& context) override { m_Context = &context; }
		void OnDetach() override { m_Context = nullptr; }
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		void OnUpdate(Timestep ts, SceneMode mode) override;

		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Animation; }

	private:
		static inline std::string s_Name = "AnimationSystem";
	};

} // namespace Lunex
