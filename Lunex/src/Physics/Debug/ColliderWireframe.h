#pragma once

/**
 * @file ColliderWireframe.h
 * @brief Line geometry for drawing mesh colliders in the editor.
 *
 * Results are cached per model (or per custom vertex set), so the hull/edge extraction
 * only runs once, not every frame.
 */

#include "Core/Core.h"
#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	class Model;

	namespace ColliderWireframe {

		// Line list (pairs of points) in model space.
		//  convex = true  -> edges of the convex hull Bullet builds for the collider
		//  convex = false -> unique triangle edges (capped for very dense meshes)
		const std::vector<glm::vec3>& GetModelEdges(const Ref<Model>& model, bool convex);

		const std::vector<glm::vec3>& GetCustomEdges(const std::vector<glm::vec3>& vertices,
			const std::vector<uint32_t>& indices, bool convex);

	} // namespace ColliderWireframe

} // namespace Lunex
