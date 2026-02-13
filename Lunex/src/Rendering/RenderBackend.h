#pragma once

/**
 * @file RenderBackend.h
 * @brief Abstract render backend interface for multi-pipeline support
 * 
 * Enables switching between rendering pipelines at runtime:
 * - Rasterization (traditional forward/deferred rendering)
 * - Ray Tracing (compute-shader based path tracing)
 * - Hybrid (raster geometry + RT shadows/reflections/GI)
 * 
 * Each backend registers its passes into the existing RenderGraph,
 * so dependency resolution, resource management, and parallel
 * execution all continue to work unchanged.
 */

#include "Core/Core.h"
#include "RenderGraph.h"
#include "RenderPass.h"

#include <string>
#include <cstdint>

namespace Lunex {

	// Forward declarations
	struct RenderSystemConfig;
	struct SceneRenderData;

	// ============================================================================
	// RENDER MODE
	// ============================================================================

	enum class RenderMode : uint8_t {
		Rasterization = 0,
		RayTracing,
		Hybrid
	};

	inline const char* RenderModeToString(RenderMode mode) {
		switch (mode) {
			case RenderMode::Rasterization: return "Rasterization";
			case RenderMode::RayTracing:    return "Ray Tracing";
			case RenderMode::Hybrid:        return "Hybrid";
			default:                        return "Unknown";
		}
	}

	// ============================================================================
	// RENDER BACKEND INTERFACE
	// ============================================================================

	/**
	 * @class IRenderBackend
	 * @brief Abstract interface for a rendering backend
	 * 
	 * Lifecycle per frame:
	 *   1. PrepareSceneData()  — upload/update GPU data (BVH, SSBOs, etc.)
	 *   2. BuildRenderGraph()  — register passes into the graph
	 *   3. (RenderSystem compiles & executes the graph)
	 */
	class IRenderBackend {
	public:
		virtual ~IRenderBackend() = default;

		// ---- Identification ----
		virtual RenderMode GetMode() const = 0;
		virtual const char* GetName() const = 0;

		// ---- Lifecycle ----
		virtual void Initialize(const RenderSystemConfig& config) = 0;
		virtual void Shutdown() = 0;
		virtual void OnViewportResize(uint32_t width, uint32_t height) = 0;

		// ---- Per-frame ----
		virtual void PrepareSceneData(const SceneRenderData& sceneData) = 0;
		virtual void BuildRenderGraph(RenderGraph& graph, const SceneRenderInfo& sceneInfo) = 0;

		// ---- Capabilities ----
		virtual bool SupportsFeature(const std::string& feature) const { return false; }
		virtual bool SupportsEditorOverlays() const { return true; }
		virtual bool RequiresAccelerationStructure() const { return false; }

		// ---- Output handles ----
		virtual RenderGraphResource GetFinalColorOutput() const = 0;
		virtual RenderGraphResource GetDepthOutput() const { return RenderGraphResource{}; }
	};

} // namespace Lunex
