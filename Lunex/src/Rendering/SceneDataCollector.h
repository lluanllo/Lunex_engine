#pragma once

/**
 * @file SceneDataCollector.h
 * @brief Fills a SceneRenderData snapshot from a live Scene
 *
 * Both raster and ray-tracing backends consume SceneRenderData.
 * This collector walks the ECS registry once per frame and produces
 * a flat list of draw items + light data.
 */

#include "SceneRenderData.h"
#include "Scene/Scene.h"
#include "Scene/Camera/EditorCamera.h"
#include "Scene/Camera/Camera.h"

namespace Lunex {

	class SceneDataCollector {
	public:
		/**
		 * @brief Collect draw items and lights from the scene (editor camera).
		 */
		static void Collect(Scene* scene,
		                    const EditorCamera& camera,
		                    SceneRenderData& outData);

		/**
		 * @brief Collect draw items and lights from the scene (runtime camera).
		 */
		static void Collect(Scene* scene,
		                    const Camera& camera,
		                    const glm::mat4& cameraTransform,
		                    SceneRenderData& outData);

	private:
		static void CollectMeshes(Scene* scene, SceneRenderData& outData);
		static void CollectLights(Scene* scene, SceneRenderData& outData);
	};

} // namespace Lunex
