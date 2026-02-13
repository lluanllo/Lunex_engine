#pragma once

/**
 * @file SceneDataCollector.h
 * @brief Collects render data from the ECS once per frame
 * 
 * Bridge between Scene (ECS) and the rendering backends.
 * Called once per frame; builds a SceneRenderData struct that
 * any backend can consume.
 */

#include "Core/Core.h"
#include "SceneRenderData.h"
#include "Scene/Camera/CameraData.h"
#include "Scene/Lighting/LightTypes.h"

namespace Lunex {

	class Scene;

	class SceneDataCollector {
	public:
		/**
		 * @brief Collect all render data from a scene
		 * 
		 * @param scene       The scene to collect from
		 * @param camera      Camera data (already resolved by RenderSystem)
		 * @param lighting    Pre-collected lighting data
		 * @param editorMode  If true, flag editor overlays
		 */
		static SceneRenderData Collect(
			Scene* scene,
			const CameraRenderData& camera,
			const LightingData& lighting,
			bool editorMode
		);
	};

} // namespace Lunex
