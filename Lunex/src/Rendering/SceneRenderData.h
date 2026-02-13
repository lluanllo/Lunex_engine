#pragma once

/**
 * @file SceneRenderData.h
 * @brief Shared data structures consumed by all render backends
 *
 * SceneRenderData is a per-frame snapshot of the scene.
 * Filled by SceneDataCollector, read by the active backend.
 */

#include "Core/Core.h"
#include "Scene/Lighting/LightTypes.h"
#include "Resources/Mesh/Model.h"
#include "Resources/Render/MaterialInstance.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Lunex {

	// ====================================================================
	// PER-OBJECT DRAW DATA
	// ====================================================================

	struct SceneDrawItem {
		glm::mat4              Transform  = glm::mat4(1.0f);
		Ref<Model>             MeshModel  = nullptr;
		Ref<MaterialInstance>  Material   = nullptr;
		int                    EntityID   = -1;
	};

	// ====================================================================
	// FULL SCENE SNAPSHOT
	// ====================================================================

	struct SceneRenderData {
		// Camera
		glm::mat4 ViewProjection      = glm::mat4(1.0f);
		glm::mat4 View                = glm::mat4(1.0f);
		glm::mat4 Projection          = glm::mat4(1.0f);
		glm::mat4 InverseView         = glm::mat4(1.0f);
		glm::mat4 InverseProjection   = glm::mat4(1.0f);
		glm::vec3 CameraPosition      = glm::vec3(0.0f);
		float     CameraNear          = 0.1f;
		float     CameraFar           = 1000.0f;

		// Draw items (3D meshes only – 2D always uses raster)
		std::vector<SceneDrawItem> DrawItems;

		// Lights (GPU-ready format)
		std::vector<LightData> Lights;

		// Viewport
		uint32_t ViewportWidth  = 0;
		uint32_t ViewportHeight = 0;

		void Clear() {
			DrawItems.clear();
			Lights.clear();
		}
	};

} // namespace Lunex
