#include "stpch.h"
#include "SceneDataCollector.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Lighting/LightSystem.h"
#include "Log/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lunex {

	// ====================================================================
	// PUBLIC API
	// ====================================================================

	void SceneDataCollector::Collect(Scene* scene,
	                                  const EditorCamera& camera,
	                                  SceneRenderData& outData) {
		outData.Clear();
		if (!scene) return;

		outData.ViewProjection    = camera.GetViewProjection();
		outData.View              = camera.GetViewMatrix();
		outData.Projection        = camera.GetProjection();
		outData.InverseView       = glm::inverse(outData.View);
		outData.InverseProjection = glm::inverse(outData.Projection);
		outData.CameraPosition    = camera.GetPosition();
		outData.CameraNear        = camera.GetNearClip();
		outData.CameraFar         = camera.GetFarClip();

		CollectMeshes(scene, outData);
		CollectLights(scene, outData);
	}

	void SceneDataCollector::Collect(Scene* scene,
	                                  const Camera& camera,
	                                  const glm::mat4& cameraTransform,
	                                  SceneRenderData& outData) {
		outData.Clear();
		if (!scene) return;

		outData.View              = glm::inverse(cameraTransform);
		outData.Projection        = camera.GetProjection();
		outData.ViewProjection    = outData.Projection * outData.View;
		outData.InverseView       = cameraTransform;
		outData.InverseProjection = glm::inverse(outData.Projection);
		outData.CameraPosition    = glm::vec3(cameraTransform[3]);
		outData.CameraNear        = 0.1f;
		outData.CameraFar         = 1000.0f;

		CollectMeshes(scene, outData);
		CollectLights(scene, outData);
	}

	// ====================================================================
	// INTERNAL
	// ====================================================================

	void SceneDataCollector::CollectMeshes(Scene* scene, SceneRenderData& outData) {
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();

		for (auto entity : view) {
			auto& mesh = view.template get<MeshComponent>(entity);
			if (!mesh.MeshModel) continue;

			Entity e = { entity, scene };

			SceneDrawItem item;
			item.Transform = scene->GetWorldTransform(e);
			item.MeshModel = mesh.MeshModel;
			item.EntityID  = static_cast<int>(entity);

			if (e.HasComponent<MaterialComponent>()) {
				auto& mat = e.GetComponent<MaterialComponent>();
				item.Material = mat.Instance;
			}

			outData.DrawItems.push_back(std::move(item));
		}
	}

	void SceneDataCollector::CollectLights(Scene* scene, SceneRenderData& outData) {
		auto view = scene->GetAllEntitiesWith<TransformComponent, LightComponent>();

		for (auto entity : view) {
			Entity e = { entity, scene };
			auto& transform = view.template get<TransformComponent>(entity);
			auto& light = view.template get<LightComponent>(entity);

			glm::mat4 worldTf  = scene->GetWorldTransform(e);
			glm::vec3 position = glm::vec3(worldTf[3]);
			glm::vec3 direction = glm::normalize(
				glm::rotate(glm::quat(transform.Rotation), glm::vec3(0.0f, 0.0f, -1.0f))
			);
			outData.Lights.push_back(light.LightInstance->GetLightData(position, direction));
		}
	}

} // namespace Lunex
