#pragma once

/**
 * @file RayTracingBackend.h
 * @brief Compute-shader path-tracing backend
 *
 * Uses a progressive accumulation approach:
 *   - Each frame dispatches the path-tracer compute shader for N samples.
 *   - Results accumulate in an RGBA32F buffer.
 *   - A second pass tone-maps into an RGBA8 output texture.
 *   - Camera or scene changes reset the accumulator.
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

		/**
		 * @brief Blit the path tracer output onto the currently-bound framebuffer.
		 *
		 * Draws a fullscreen triangle using the tone-mapped RGBA8 output texture.
		 * Then performs a fast raster Entity-ID pass so mouse picking works.
		 * Must be called while the editor framebuffer is bound.
		 */
		void BlitToFramebuffer();

	private:
		void CreateOutputTextures(uint32_t w, uint32_t h);
		void UpdateCameraUBO();
		void UploadLights();
		bool DetectSceneChanges();
		bool DetectIBLChanges();
		size_t ComputeSceneHash() const;

		/**
		 * @brief Fast raster pass that writes entity IDs to the RED_INTEGER attachment.
		 *
		 * The path tracer compute shader cannot write entity IDs, so this pass
		 * re-renders all scene meshes with a minimal shader that only outputs
		 * o_EntityID.  Depth testing is enabled so only visible fragments win.
		 * Color writes are disabled to preserve the path-traced image.
		 */
		void RenderEntityIDPass();

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

		// Camera UBO (binding = 15, std140 layout)
		// Total = 64+64+16 + 4*8 + 4 + 4+4+4 + 16 + 4+4+8 = 224 bytes (std140 aligned)
		struct CameraUBOData {
			glm::mat4 InverseProjection;   // 64   offset 0
			glm::mat4 InverseView;         // 64   offset 64
			glm::vec4 CameraPosition;      // 16   offset 128
			uint32_t  FrameIndex;          // 4    offset 144
			uint32_t  SampleCount;         // 4    offset 148
			uint32_t  MaxBounces;          // 4    offset 152
			uint32_t  SamplesPerFrame;     // 4    offset 156
			uint32_t  TriangleCount;       // 4    offset 160
			uint32_t  BVHNodeCount;        // 4    offset 164
			uint32_t  LightCount;          // 4    offset 168
			uint32_t  MaterialCount;       // 4    offset 172
			float     RussianRoulette;     // 4    offset 176
			float     IBLRotation;         // 4    offset 180  (radians)
			float     IBLIntensity;        // 4    offset 184
			float     DenoiserStrength;    // 4    offset 188
			glm::vec4 IBLTint;            // 16   offset 192  (xyz=tint, w=enableDenoiser) ? 208 total
		};

		Ref<UniformBuffer> m_CameraUBO;
		CameraUBOData      m_CameraData{};

		// Lights SSBO (grow-on-demand, re-uploaded each frame)
		Ref<StorageBuffer> m_LightsSSBO;
		uint32_t           m_LightsSSBOCapacity = 0;

		// State
		uint32_t m_SampleCount = 0;
		uint32_t m_FrameIndex  = 0;
		uint32_t m_Width       = 0;
		uint32_t m_Height      = 0;
		glm::mat4 m_LastVP     = glm::mat4(1.0f);
		size_t    m_LastSceneHash = 0;

		// IBL state tracking (detect changes ? reset accumulation)
		float     m_LastIBLRotation  = 0.0f;
		float     m_LastIBLIntensity = 1.0f;
		glm::vec3 m_LastIBLTint      = glm::vec3(1.0f);
		bool      m_LastHasHDRI      = false;

		// Fullscreen blit resources (for compositing PT output onto the editor FBO)
		Ref<Shader> m_BlitShader;
		uint32_t    m_BlitDummyVAO = 0;  // empty VAO for attribute-less rendering

		// Entity ID pass resources (for mouse picking in PT mode)
		Ref<Shader>        m_EntityIDShader;
		Ref<UniformBuffer> m_EntityIDCameraUBO;   // binding 0: ViewProjection
		Ref<UniformBuffer> m_EntityIDTransformUBO; // binding 1: Transform
		Ref<UniformBuffer> m_EntityIDEntityUBO;    // binding 2: EntityData (entity ID)
	};

} // namespace Lunex
