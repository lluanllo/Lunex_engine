#include "stpch.h"
#include "BlueprintSystem.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scripting/Blueprint/Blueprint.h"
#include "Project/Project.h"
#include "Log/Log.h"

namespace Lunex {

	namespace {
		std::filesystem::path Resolve(const std::string& stored) {
			std::filesystem::path p(stored);
			if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
			return p;
		}
	}

	void BlueprintSystem::OnRuntimeStart(SceneMode mode) {
		if (!IsActiveInMode(mode) || !m_Context || !m_Context->Registry) return;
		Scene* scene = m_Context->OwningScene;

		// Collect first: OnStart graphs may create entities
		std::vector<entt::entity> owners;
		auto view = m_Context->Registry->view<ScriptComponent>();
		for (auto e : view)
			if (!view.get<ScriptComponent>(e).BlueprintPath.empty()) owners.push_back(e);

		for (auto e : owners) {
			if (!m_Context->Registry->valid(e)) continue;
			auto& sc = m_Context->Registry->get<ScriptComponent>(e);
			auto asset = Blueprint::BlueprintAsset::GetOrLoad(Resolve(sc.BlueprintPath));
			if (!asset) { LNX_LOG_WARN("Blueprint not found: {0}", sc.BlueprintPath); continue; }
			auto instance = std::make_shared<Blueprint::Instance>(asset);
			sc.BlueprintRuntime = instance;
			instance->Start(scene, Entity{ e, scene });
		}
		m_Running = true;
	}

	void BlueprintSystem::OnRuntimeStop() {
		if (m_Context && m_Context->Registry) {
			auto view = m_Context->Registry->view<ScriptComponent>();
			for (auto e : view) view.get<ScriptComponent>(e).BlueprintRuntime.reset();
		}
		m_Running = false;
	}

	void BlueprintSystem::OnUpdate(Timestep ts, SceneMode mode) {
		if (!m_Running || !IsActiveInMode(mode) || !m_Context || !m_Context->Registry) return;
		Scene* scene = m_Context->OwningScene;

		std::vector<std::pair<entt::entity, std::shared_ptr<Blueprint::Instance>>> running;
		auto view = m_Context->Registry->view<ScriptComponent>();
		for (auto e : view) {
			auto& sc = view.get<ScriptComponent>(e);
			if (sc.BlueprintRuntime) running.emplace_back(e, std::static_pointer_cast<Blueprint::Instance>(sc.BlueprintRuntime));
		}
		for (auto& [e, instance] : running) {
			if (!m_Context->Registry->valid(e)) continue;   // destroyed by another blueprint this frame
			instance->Update(scene, Entity{ e, scene }, static_cast<float>(ts));
		}
	}

} // namespace Lunex
