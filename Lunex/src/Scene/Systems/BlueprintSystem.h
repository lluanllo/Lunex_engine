#pragma once

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

namespace Lunex {

	/**
	 * @class BlueprintSystem
	 * @brief Runs the visual scripts (ScriptComponent::BlueprintPath) in Play mode.
	 */
	class BlueprintSystem : public ISceneSystem {
	public:
		void OnAttach(SceneContext& context) override { m_Context = &context; }
		void OnDetach() override { m_Context = nullptr; }
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		void OnUpdate(Timestep ts, SceneMode mode) override;

		bool IsActiveInMode(SceneMode mode) const override { return mode == SceneMode::Play; }
		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Script; }

	private:
		bool m_Running = false;
		static inline std::string s_Name = "BlueprintSystem";
	};

} // namespace Lunex
