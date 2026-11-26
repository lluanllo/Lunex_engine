#pragma once

#include "Scene/Core/ISceneSystem.h"
#include "Scene/Core/SceneContext.h"
#include "Core/Core.h"

#include <glm/glm.hpp>

namespace Lunex {

	class Scene;

	/**
	 * @class ParticleSystem
	 * @brief Simulates ParticleSystemComponents (see Components.h).
	 *
	 * Simple simulation runs on the CPU (also in Edit mode when PreviewInEditor is on).
	 * Physics simulation and prefab particles spawn real entities, so they only run in
	 * Play/Simulate. Rendering is done by SceneRenderSystem through the static helpers.
	 */
	class ParticleSystem : public ISceneSystem {
	public:
		void OnAttach(SceneContext& context) override { m_Context = &context; }
		void OnDetach() override { m_Context = nullptr; }
		void OnRuntimeStart(SceneMode mode) override;
		void OnRuntimeStop() override;
		void OnUpdate(Timestep ts, SceneMode mode) override;

		bool IsActiveInMode(SceneMode mode) const override { return mode != SceneMode::Paused; }
		const std::string& GetName() const override { return s_Name; }
		SceneSystemPriority GetPriority() const override { return SceneSystemPriority::Animation; }

		// ---- Rendering helpers (called by SceneRenderSystem) ----
		// Mesh particles of the Simple simulation, inside the geometry pass
		static void SubmitMeshParticles(Scene* scene, entt::registry& registry, bool deferred);
		// Sprite particles, after lighting (inside Renderer2D::BeginScene/EndScene)
		static void DrawBillboards(Scene* scene, entt::registry& registry, const glm::vec3& cameraPosition);

	private:
		static inline std::string s_Name = "ParticleSystem";
	};

} // namespace Lunex
