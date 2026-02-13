#pragma once

/**
 * @file SceneRenderData.h
 * @brief Backend-agnostic scene data for rendering
 * 
 * Collected once per frame by SceneDataCollector and consumed by
 * whichever IRenderBackend is active. The renderer never queries
 * the ECS directly — all scene data flows through this struct.
 */

#include "Core/Core.h"
#include "Scene/Camera/CameraData.h"
#include "Scene/Lighting/LightTypes.h"
#include "Renderer/EnvironmentMap.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Lunex {

	// Forward declarations
	class Scene;

	// ============================================================================
	// SCENE RENDER DATA
	// ============================================================================

	/**
	 * @struct SceneRenderData
	 * @brief Complete scene data collected for one frame of rendering
	 * 
	 * This is backend-agnostic: raster, ray tracing, and hybrid backends
	 * all receive the same SceneRenderData and extract what they need.
	 */
	struct SceneRenderData {
		// ---- Source ----
		Scene* SourceScene = nullptr;

		// ---- Camera ----
		CameraRenderData Camera;

		// ---- Lighting ----
		LightingData Lighting;

		// ---- Environment ----
		Ref<EnvironmentMap> Environment;

		// ---- Editor state ----
		bool DrawGrid = false;
		bool DrawGizmos = false;
		bool DrawBounds = false;
		int  SelectedEntityID = -1;

		// ---- Statistics ----
		uint32_t TotalMeshes = 0;
		uint32_t CulledMeshes = 0;
	};

} // namespace Lunex
