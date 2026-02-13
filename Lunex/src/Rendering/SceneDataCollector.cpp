#include "stpch.h"
#include "SceneDataCollector.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/SkyboxRenderer.h"

namespace Lunex {

	SceneRenderData SceneDataCollector::Collect(
		Scene* scene,
		const CameraRenderData& camera,
		const LightingData& lighting,
		bool editorMode)
	{
		SceneRenderData data;
		if (!scene) return data;

		data.SourceScene = scene;
		data.Camera      = camera;
		data.Lighting    = lighting;
		data.DrawGrid    = editorMode;
		data.DrawGizmos  = editorMode;

		// Mesh count (lightweight — just count, passes still iterate themselves)
		{
			auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
			for (auto entityID : view) {
				Entity entity = { entityID, scene };
				auto& mesh = entity.GetComponent<MeshComponent>();
				if (mesh.MeshModel && !mesh.MeshModel->GetMeshes().empty())
					data.TotalMeshes++;
			}
		}

		// Environment from global skybox
		data.Environment = SkyboxRenderer::GetGlobalEnvironment();

		return data;
	}

} // namespace Lunex
