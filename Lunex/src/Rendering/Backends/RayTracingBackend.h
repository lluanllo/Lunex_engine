#pragma once

/**
 * @file RayTracingBackend.h
 * @brief Compute-shader path-tracing backend (Phase 2)
 *
 * Uses a single compute shader that writes to an image2D.
 * BVH traversal + Monte Carlo path tracing run entirely on the GPU
 * via OpenGL 4.3+ compute shaders.
 *
 * Per-frame pipeline:
 *   1. PrepareSceneData — rebuild BVH if scene changed, update camera UBO
 *   2. BuildRenderGraph — add PathTrace (compute) + Composite (fullscreen) passes
 *   3. Graph compile + execute
 */

#include "Rendering/RenderBackend.h"
#include "RayTracingScene.h"
#include "Renderer/Shader.h"

namespace Lunex {

	// ============================================================================
	// RAY TRACING CONFIGURATION
	// ============================================================================

	struct RayTracingConfig {
		int   MaxBounces = 4;
		bool  AccumulateFrames = true;
		float Exposure = 1.0f;
	};

	// ============================================================================
	// RAY TRACING BACKEND
	// ============================================================================

	class RayTracingBackend : public IRenderBackend {
	public:
		RayTracingBackend() = default;
		~RayTracingBackend() override = default;

		// ---- IRenderBackend ----
		RenderMode  GetMode() const override { return RenderMode::RayTracing; }
		const char* GetName() const override { return "Ray Tracing (Compute)"; }

		void Initialize(const RenderSystemConfig& config) override;
		void Shutdown() override;
		void OnViewportResize(uint32_t width, uint32_t height) override;

		void PrepareSceneData(const SceneRenderData& sceneData) override;
		void BuildRenderGraph(RenderGraph& graph, const SceneRenderInfo& sceneInfo) override;

		bool SupportsFeature(const std::string& feature) const override;
		bool SupportsEditorOverlays() const override { return false; }
		bool RequiresAccelerationStructure() const override { return true; }

		RenderGraphResource GetFinalColorOutput() const override { return m_FinalColorTarget; }

		// ---- RT-specific API ----
		RayTracingConfig& GetConfig() { return m_Config; }
		const RayTracingConfig& GetConfig() const { return m_Config; }
		void ResetAccumulation() { m_FrameIndex = 0; }

		uint32_t GetAccumulatedFrames() const { return m_FrameIndex; }

	private:
		void CreateOutputTexture(uint32_t w, uint32_t h);
		void CreateAccumulationTexture(uint32_t w, uint32_t h);

		// Shaders
		Ref<Shader> m_PathTraceShader;
		Ref<Shader> m_CompositeShader;

		// Output
		Ref<RHI::RHITexture2D> m_OutputTexture;       // RGBA32F — compute writes here
		Ref<RHI::RHITexture2D> m_AccumulationTexture;  // RGBA32F — running average

		// Scene acceleration structure
		RayTracingSceneData m_SceneData;

		// Render graph handle
		RenderGraphResource m_FinalColorTarget;

		// Frame state
		uint32_t m_FrameIndex = 0;
		uint32_t m_ViewportWidth  = 1;
		uint32_t m_ViewportHeight = 1;
		bool     m_SceneDirty = true;
		bool     m_Initialized = false;

		RayTracingConfig m_Config;
	};

} // namespace Lunex
