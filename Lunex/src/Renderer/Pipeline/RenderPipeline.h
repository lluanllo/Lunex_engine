#pragma once

/**
 * @file RenderPipeline.h
 * @brief Selectable render engines (Blender-style "Render Engine" setting).
 *
 * Each pipeline declares how it renders and which shader set it loads:
 *   - Lunex Raster:      the current deferred rasterizer (G-Buffer, CSM, IBL, SSR...)
 *   - Lunex Path Tracer: progressive unidirectional path tracer, Cycles-like (planned)
 *
 * The active pipeline is stored per project (ProjectConfig::RenderEngine). Pipelines
 * that are not implemented yet report IsAvailable() == false and the registry falls
 * back to the rasterizer, so projects that select them keep working.
 *
 * Adding a pipeline: implement IRenderPipeline, register it in RenderPipelineRegistry
 * (RenderPipeline.cpp) and route SceneRenderSystem to it when it is active.
 */

#include <string>
#include <vector>

namespace Lunex {

	enum class RenderEngineType : uint8_t {
		Rasterizer = 0,
		PathTracer = 1
	};

	const char* RenderEngineToString(RenderEngineType type);
	RenderEngineType RenderEngineFromString(const std::string& s);

	struct RenderPipelineFeatures {
		bool Realtime = true;
		bool Progressive = false;        // accumulates samples over frames
		bool GlobalIllumination = false; // physically based GI
		bool ScreenSpaceEffects = true;  // SSR/SSAO style approximations
	};

	class IRenderPipeline {
	public:
		virtual ~IRenderPipeline() = default;

		virtual RenderEngineType GetType() const = 0;
		virtual const char* GetName() const = 0;
		virtual const char* GetDescription() const = 0;
		virtual bool IsAvailable() const = 0;
		virtual RenderPipelineFeatures GetFeatures() const = 0;

		// Shader set this pipeline loads (relative to the editor working directory)
		virtual const char* GetShaderDirectory() const = 0;

		// Called when the pipeline becomes / stops being the active one
		virtual void OnActivate() {}
		virtual void OnDeactivate() {}
	};

	class RenderPipelineRegistry {
	public:
		static const std::vector<IRenderPipeline*>& GetAll();
		static IRenderPipeline* Get(RenderEngineType type);

		// Requested engine (what the project asks for) vs active (what actually renders)
		static void SetRequested(RenderEngineType type);
		static RenderEngineType GetRequested();
		static IRenderPipeline* GetActive();
	};

} // namespace Lunex
