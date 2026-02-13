#pragma once

/**
 * @file RenderBackend.h
 * @brief Abstract render backend interface
 *
 * Every rendering backend (rasterizer, path tracer, ...) implements this.
 * The active backend is managed by SceneRenderSystem.
 *
 * Design:
 * 1. RasterBackend wraps existing Renderer3D — zero changes to raster code.
 * 2. RayTracingBackend uses compute shaders + BVH.
 * 3. Switching is a pointer swap — no scene rebuild required.
 * 4. 2D rendering (sprites, gizmos, grid) is ALWAYS raster.
 */

#include "Core/Core.h"
#include "SceneRenderData.h"

#include <glm/glm.hpp>
#include <string>
#include <cstdint>

namespace Lunex {

	class Scene;
	class EditorCamera;
	class Camera;

	// ====================================================================
	// BACKEND TYPE
	// ====================================================================

	enum class RenderBackendType : uint8_t {
		Rasterizer = 0,
		PathTracer = 1
	};

	inline const char* RenderBackendTypeToString(RenderBackendType t) {
		switch (t) {
			case RenderBackendType::Rasterizer: return "Rasterizer";
			case RenderBackendType::PathTracer: return "Path Tracer";
		}
		return "Unknown";
	}

	// ====================================================================
	// SETTINGS (shared + per-backend)
	// ====================================================================

	struct RenderBackendSettings {
		// Path Tracer
		uint32_t MaxBounces             = 4;
		uint32_t SamplesPerFrame        = 1;
		uint32_t MaxAccumulatedSamples  = 0;       // 0 = infinite
		float    RussianRouletteThresh  = 0.01f;
	};

	// ====================================================================
	// STATISTICS
	// ====================================================================

	struct RenderBackendStats {
		uint32_t DrawCalls          = 0;
		uint32_t TriangleCount      = 0;
		uint32_t MeshCount          = 0;
		// Path tracer
		uint32_t AccumulatedSamples = 0;
		uint32_t BVHNodeCount       = 0;
		uint32_t TotalTriangles     = 0;
		float    LastFrameTimeMs    = 0.0f;
	};

	// ====================================================================
	// ABSTRACT BACKEND
	// ====================================================================

	class RenderBackend {
	public:
		virtual ~RenderBackend() = default;

		virtual RenderBackendType GetType() const = 0;
		virtual const char*      GetName() const  = 0;

		// -- lifecycle ---------------------------------------------------
		virtual void Initialize() = 0;
		virtual void Shutdown()   = 0;

		// -- per-frame (editor) ------------------------------------------
		virtual void BeginFrame(const EditorCamera& camera) = 0;
		virtual void RenderScene(Scene* scene) = 0;
		virtual void EndFrame() = 0;

		// -- per-frame (runtime) -----------------------------------------
		virtual void BeginFrameRuntime(const Camera& camera,
		                               const glm::mat4& cameraTransform) = 0;
		// RenderScene / EndFrame shared

		// -- scene notifications -----------------------------------------
		virtual void OnSceneChanged(Scene* scene) = 0;

		// -- output ------------------------------------------------------
		// Returns 0 when the viewport already has the texture (raster).
		virtual uint32_t GetOutputTextureID() const = 0;

		// -- viewport ----------------------------------------------------
		virtual void OnViewportResize(uint32_t w, uint32_t h) = 0;

		// -- settings / stats --------------------------------------------
		virtual void                        SetSettings(const RenderBackendSettings& s) { m_Settings = s; }
		virtual const RenderBackendSettings& GetSettings() const { return m_Settings; }

		virtual bool     IsProgressiveRender()   const { return false; }
		virtual uint32_t GetAccumulatedSamples() const { return 0; }
		virtual void     ResetAccumulation()           {}

		virtual RenderBackendStats GetStats() const = 0;

	protected:
		RenderBackendSettings m_Settings;
	};

} // namespace Lunex
