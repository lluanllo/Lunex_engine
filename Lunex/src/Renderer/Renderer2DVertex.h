#pragma once

/**
 * @file Renderer2DVertex.h
 * @brief Vertex layouts of the Renderer2D batches (shared by the GL path and the Vulkan
 *        scene renderer, which draws the same batches with its own pipelines).
 */

#include <glm/glm.hpp>

namespace Lunex {

	struct QuadVertex {
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;
		int EntityID;
	};

	struct CircleVertex {
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;
		int EntityID;   // editor-only
	};

	struct LineVertex {
		glm::vec3 Position;
		glm::vec4 Color;
		int EntityID;   // editor-only
	};

	static_assert(sizeof(QuadVertex) == 48, "QuadVertex layout is mirrored by the Vulkan 2D pipelines");
	static_assert(sizeof(CircleVertex) == 52, "CircleVertex layout is mirrored by the Vulkan 2D pipelines");
	static_assert(sizeof(LineVertex) == 32, "LineVertex layout is mirrored by the Vulkan 2D pipelines");

}
