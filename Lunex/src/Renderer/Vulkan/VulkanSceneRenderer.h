#pragma once

/**
 * @file VulkanSceneRenderer.h
 * @brief Native Vulkan scene renderer (used when the Vulkan backend is active).
 *
 * Draws the scene with prebuilt pipelines and GPU-resident data instead of emulating the
 * OpenGL renderers call by call:
 *  - meshes (static + skinned) uploaded once to device-local buffers, frustum culled
 *  - PBR (GGX + Burley) with directional / point / spot lights, metal-rough maps, normal
 *    maps, packed ORM, emission
 *  - image based lighting from the scene HDRI (equirect, mipmapped by roughness), sky
 *  - sun shadow map (4096, texel-snapped ortho fit to the view, 5x5 PCF)
 *  - editor grid, selection outline, entity IDs for picking (async readback, one frame late)
 * Output is an RGBA8 image in GL row order (standard Vulkan viewport), shown by the viewport like the GL framebuffer.
 */

#include "Core/Core.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Lunex {

	class Scene;

	struct VulkanRenderView {
		glm::mat4 View{ 1.0f };
		glm::mat4 Projection{ 1.0f };
		glm::vec3 CameraPosition{ 0.0f };
		uint32_t Width = 1, Height = 1;
		bool DrawGrid = true;
		bool DrawSky = true;
		bool Shadows = true;
		// Entity IDs drawn with the selection outline
		std::vector<int> Selected;
		// Mouse position in pixels (GL convention: origin bottom-left), -1 = none
		int PickX = -1, PickY = -1;
	};

	class VulkanSceneRenderer {
	public:
		static VulkanSceneRenderer& Get();

		// Records the scene into the current frame's command buffer
		void Render(Scene& scene, const VulkanRenderView& view);
		// ImGui texture ID of the last rendered image (0 = none yet)
		uint32_t GetOutputTextureID() const;
		// Entity under the pick position of a previous frame (-1 = none)
		int GetPickedEntity() const { return m_PickedEntity; }

		struct Statistics { uint32_t DrawCalls = 0, ShadowDrawCalls = 0, Triangles = 0, Culled = 0, Meshes = 0, Lights = 0; float CpuMs = 0.0f; float GpuMs = -1.0f; };
		const Statistics& GetStats() const { return m_Stats; }

	private:
		VulkanSceneRenderer() = default;
		int m_PickedEntity = -1;
		Statistics m_Stats;
	};

}
