#pragma once

/**
 * @file RayTracingBackend.h
 * @brief Compute-shader path-tracing backend
 *
 * Uses a progressive accumulation approach:
 *   - Each frame dispatches the path-tracer compute shader for 1 sample.
 *   - Results accumulate in an RGBA32F buffer.
 *   - A second pass tone-maps into an RGBA8 output texture.
 *   - Camera movement resets the accumulator.
 */

#include "Rendering/RenderBackend.h"
#include "Rendering/SceneDataCollector.h"
#include "RayTracingScene.h"

#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/StorageBuffer.h"
#include "RHI/RHI.h"

namespace Lunex {

	class RayTracingBackend : public RenderBackend {
	public:
		RayTracingBackend() = default;
		~RayTracingBackend() override = default;

		RenderBackendType GetType() const override { return RenderBackendType::PathTracer; }
		const char*       GetName() const override { return "Path Tracer"; }

		void Initialize() override;
		void Shutdown()   override;

		void BeginFrame(const EditorCamera& camera) override;
		void RenderScene(Scene* scene) override;
		void EndFrame() override;

		void BeginFrameRuntime(const Camera& camera,
		                       const glm::mat4& cameraTransform) override;

		void OnSceneChanged(Scene* scene) override;

		uint32_t GetOutputTextureID() const override;

		void OnViewportResize(uint32_t w, uint32_t h) override;

		// Progressive rendering
		bool     IsProgressiveRender()   const override { return true; }
		uint32_t GetAccumulatedSamples() const override { return m_SampleCount; }
		void     ResetAccumulation()           override;

		RenderBackendStats GetStats() const override;

	private:
		void CreateOutputTextures(uint32_t w, uint32_t h);
		void UpdateCameraUBO();

	private:
		// Scene
		RayTracingScene m_RTScene;
		SceneRenderData m_SceneData;
		Scene*          m_CurrentScene = nullptr;

		// Compute shader
		Ref<RHI::RHIShader> m_PathTracerShader;

		// Output images
		uint32_t m_AccumulationTexID = 0;  // GL texture handle, RGBA32F
		uint32_t m_OutputTexID       = 0;  // GL texture handle, RGBA8

		// Camera UBO (binding = 15)
		struct CameraUBOData {
			glm::mat4 InverseProjection;   // 64
			glm::mat4 InverseView;         // 64
			glm::vec4 CameraPosition;      // 16
			uint32_t  FrameIndex;          // 4
			uint32_t  SampleCount;         // 4
			uint32_t  MaxBounces;          // 4
			uint32_t  TriangleCount;       // 4
			uint32_t  BVHNodeCount;        // 4
			uint32_t  LightCount;          // 4
			uint32_t  MaterialCount;       // 4
			float     RussianRoulette;     // 4   = 176 total
		};

		Ref<UniformBuffer> m_CameraUBO;
		CameraUBOData      m_CameraData{};

		// Lights SSBO (re-uploaded each frame)
		Ref<StorageBuffer> m_LightsSSBO;

		// State
		uint32_t m_SampleCount = 0;
		uint32_t m_FrameIndex  = 0;
		uint32_t m_Width       = 0;
		uint32_t m_Height      = 0;
		glm::mat4 m_LastVP     = glm::mat4(1.0f);
	};

} // namespace Lunex
