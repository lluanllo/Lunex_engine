#pragma once

/**
 * @file RasterBackend.h
 * @brief Rasterization backend — wraps existing Renderer3D
 *
 * This backend delegates ALL work to the existing rendering pipeline.
 * No changes are made to Renderer3D, ShadowSystem, SkyboxRenderer, etc.
 * The output texture is whatever the viewport framebuffer already has.
 */

#include "Rendering/RenderBackend.h"

namespace Lunex {

	class RasterBackend : public RenderBackend {
	public:
		RasterBackend() = default;
		~RasterBackend() override = default;

		RenderBackendType GetType() const override { return RenderBackendType::Rasterizer; }
		const char*       GetName() const override { return "Rasterizer"; }

		void Initialize() override;
		void Shutdown()   override;

		void BeginFrame(const EditorCamera& camera) override;
		void RenderScene(Scene* scene) override;
		void EndFrame() override;

		void BeginFrameRuntime(const Camera& camera,
		                       const glm::mat4& cameraTransform) override;

		void OnSceneChanged(Scene* scene) override;

		// Raster writes to the viewport FBO directly — no standalone texture.
		uint32_t GetOutputTextureID() const override { return 0; }

		void OnViewportResize(uint32_t w, uint32_t h) override;

		RenderBackendStats GetStats() const override;

	private:
		Scene*       m_CurrentScene = nullptr;
		bool         m_EditorMode   = true;

		// Cached from the last BeginFrame to pass into RenderScene
		const EditorCamera* m_EditorCamera   = nullptr;
		const Camera*       m_RuntimeCamera  = nullptr;
		glm::mat4           m_RuntimeCameraTransform = glm::mat4(1.0f);
	};

} // namespace Lunex
