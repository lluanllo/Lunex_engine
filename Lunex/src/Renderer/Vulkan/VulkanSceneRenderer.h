#pragma once

/**
 * @file VulkanSceneRenderer.h
 * @brief Native Vulkan scene renderer (used when the Vulkan backend is active).
 *
 * Draws the scene with prebuilt pipelines and GPU-resident data instead of emulating the
 * OpenGL renderers call by call. Frame of one view:
 *  1. sun shadow map (4096, texel-snapped ortho fit to the view, 5x5 PCF)
 *  2. opaque HDR pass: meshes (classic PBR materials and node-graph materials, static and
 *     skinned), sky; entity IDs for picking
 *  3. transparent HDR pass: glass / transmission / blended graph materials (sorted, with
 *     screen-space refraction of a mipmapped copy of the opaque scene), Renderer2D sprites,
 *     circles and particles
 *  4. bloom (13-tap downsample chain + tent upsample)
 *  5. composite to the 8-bit output: exposure, tone mapping, gamma, vignette, chromatic
 *     aberration; then the editor grid, Renderer2D overlays (icons, gizmos, frustums,
 *     colliders) and the selection outline
 * Output is an RGBA8 image in GL row order (standard Vulkan viewport), shown by the viewport
 * like the GL framebuffer.
 */

#include "Core/Core.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Lunex {

	class Scene;
	class Texture2D;

	struct VulkanRenderView {
		glm::mat4 View{ 1.0f };
		glm::mat4 Projection{ 1.0f };
		glm::vec3 CameraPosition{ 0.0f };
		uint32_t Width = 1, Height = 1;
		// 0 = editor viewport, 1 = camera preview (separate targets)
		uint32_t ViewID = 0;
		bool DrawGrid = true;
		bool DrawSky = true;
		bool Shadows = true;
		bool PostProcess = true;
		// Draw the Renderer2D batches submitted since the last Render (sprites, overlays)
		bool Draw2D = true;
		// Entity IDs drawn with the selection outline
		std::vector<int> Selected;
		// Mouse position in pixels (GL convention: origin bottom-left), -1 = none
		int PickX = -1, PickY = -1;
	};

	// Renderer2D batch captured under Vulkan (vertices in world space, drawn by the next Render)
	struct Vulkan2DBatch {
		enum class Layer { Scene, Overlay };   // Scene: HDR, before bloom; Overlay: after tone mapping
		Layer DrawLayer = Layer::Overlay;
		glm::mat4 ViewProjection{ 1.0f };
		std::vector<uint8_t> Quads, Circles, Lines;   // QuadVertex / CircleVertex / LineVertex arrays
		uint32_t QuadCount = 0, CircleCount = 0, LineVertexCount = 0;
		std::vector<Ref<Texture2D>> Textures;          // quad TexIndex -> texture (null = white)
		float LineWidth = 1.0f;
		bool Additive = false;
		bool DepthTest = true;
		bool DepthWrite = true;
	};

	class VulkanSceneRenderer {
	public:
		static VulkanSceneRenderer& Get();

		// Records the scene into the current frame's command buffer
		void Render(Scene& scene, const VulkanRenderView& view);
		// ImGui texture ID of the last image rendered for a view (0 = none yet)
		uint32_t GetOutputTextureID(uint32_t viewID = 0) const;
		// Entity under the pick position of the editor view, a frame or two late (-1 = none)
		int GetPickedEntity() const { return m_PickedEntity; }

		// ---- Renderer2D capture ----
		void Submit2D(Vulkan2DBatch&& batch);
		void Set2DLayer(Vulkan2DBatch::Layer layer) { m_2DLayer = layer; }
		Vulkan2DBatch::Layer Get2DLayer() const { return m_2DLayer; }
		void Clear2D();

		struct Statistics {
			uint32_t DrawCalls = 0, ShadowDrawCalls = 0, Triangles = 0, Culled = 0, Meshes = 0, Lights = 0;
			uint32_t GraphDraws = 0, TransparentDraws = 0, Batches2D = 0;
			float CpuMs = 0.0f;
			float GpuMs = -1.0f;
		};
		// Statistics of the editor view (view 0)
		const Statistics& GetStats() const { return m_Stats; }

	private:
		VulkanSceneRenderer() = default;
		int m_PickedEntity = -1;
		Statistics m_Stats;
		Vulkan2DBatch::Layer m_2DLayer = Vulkan2DBatch::Layer::Overlay;
		std::vector<Vulkan2DBatch> m_2DBatches;
	};

}
