#pragma once

/**
 * @file PostProcessRenderer.h
 * @brief Post-Processing Pipeline for Deferred Rendering
 * 
 * Effects:
 *   1. Bloom (multi-pass downsample/upsample with threshold)
 *   2. Vignette
 *   3. Chromatic Aberration
 *   4. Final composite with tone mapping + gamma correction
 * 
 * The pipeline reads from the lighting pass output and writes to the scene framebuffer.
 */

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/FrameBuffer.h"

#include <glm/glm.hpp>

namespace Lunex {

	/**
	 * @struct PostProcessConfig
	 * @brief Configuration for all post-processing effects
	 */
	struct PostProcessConfig {
		// ========== BLOOM ==========
		bool EnableBloom = false;
		float BloomThreshold = 1.0f;
		float BloomIntensity = 0.5f;
		float BloomRadius = 1.0f;       // Controls blur kernel spread
		int   BloomMipLevels = 5;        // Number of downsample passes (3-8)

		// ========== VIGNETTE ==========
		bool  EnableVignette = false;
		float VignetteIntensity = 0.3f;
		float VignetteRoundness = 1.0f;
		float VignetteSmoothness = 0.4f;

		// ========== CHROMATIC ABERRATION ==========
		bool  EnableChromaticAberration = false;
		float ChromaticAberrationIntensity = 0.005f;

		// ========== TONE MAPPING ==========
		// 0=ACES, 1=Reinhard, 2=Uncharted2, 3=None
		int   ToneMapOperator = 0;
		float Exposure = 1.0f;
		float Gamma = 2.2f;
	};

	/**
	 * @class PostProcessRenderer
	 * @brief Manages post-processing effects after the deferred lighting pass
	 */
	class PostProcessRenderer {
	public:
		static void Init();
		static void Shutdown();

		/**
		 * @brief Execute all enabled post-processing effects
		 * @param sceneColorTexID OpenGL texture handle of the scene HDR color output
		 * @param targetFramebuffer The framebuffer to write the final result to
		 * @param width Viewport width
		 * @param height Viewport height
		 */
		static void Execute(uint32_t sceneColorTexID, const Ref<Framebuffer>& targetFramebuffer,
							uint32_t width, uint32_t height);

		/**
		 * @brief Resize internal resources
		 */
		static void OnViewportResize(uint32_t width, uint32_t height);

		/**
		 * @brief Access configuration
		 */
		static PostProcessConfig& GetConfig();

		/**
		 * @brief Check if system is ready
		 */
		static bool IsInitialized();

	private:
		PostProcessRenderer() = delete;

		static void ExecuteBloom(uint32_t sceneColorTexID, uint32_t width, uint32_t height);
		static void CreateBloomResources(uint32_t width, uint32_t height);
	};

} // namespace Lunex
