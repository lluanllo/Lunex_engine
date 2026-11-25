#include "stpch.h"
#include "RenderPipeline.h"

#include "Log/Log.h"

namespace Lunex {

	const char* RenderEngineToString(RenderEngineType type) {
		switch (type) {
			case RenderEngineType::PathTracer: return "PathTracer";
			default: return "Rasterizer";
		}
	}

	RenderEngineType RenderEngineFromString(const std::string& s) {
		return s == "PathTracer" ? RenderEngineType::PathTracer : RenderEngineType::Rasterizer;
	}

	namespace {
		class RasterPipeline final : public IRenderPipeline {
		public:
			RenderEngineType GetType() const override { return RenderEngineType::Rasterizer; }
			const char* GetName() const override { return "Lunex Raster"; }
			const char* GetDescription() const override {
				return "Real-time deferred rasterizer: PBR, cascaded shadows, IBL, SSR, SSS and post-processing.";
			}
			bool IsAvailable() const override { return true; }
			RenderPipelineFeatures GetFeatures() const override { return { true, false, false, true }; }
			const char* GetShaderDirectory() const override { return "assets/shaders"; }
		};

		class PathTracerPipeline final : public IRenderPipeline {
		public:
			RenderEngineType GetType() const override { return RenderEngineType::PathTracer; }
			const char* GetName() const override { return "Lunex Path Tracer"; }
			const char* GetDescription() const override {
				return "Progressive physically based path tracer (Cycles-like): global illumination, soft shadows, "
					"true reflections/refractions. Planned - the rasterizer renders meanwhile.";
			}
			bool IsAvailable() const override { return false; }
			RenderPipelineFeatures GetFeatures() const override { return { false, true, true, false }; }
			const char* GetShaderDirectory() const override { return "assets/shaders/pathtracer"; }
		};

		RasterPipeline s_Raster;
		PathTracerPipeline s_PathTracer;
		RenderEngineType s_Requested = RenderEngineType::Rasterizer;
	}

	const std::vector<IRenderPipeline*>& RenderPipelineRegistry::GetAll() {
		static std::vector<IRenderPipeline*> all = { &s_Raster, &s_PathTracer };
		return all;
	}

	IRenderPipeline* RenderPipelineRegistry::Get(RenderEngineType type) {
		for (auto* p : GetAll()) if (p->GetType() == type) return p;
		return &s_Raster;
	}

	void RenderPipelineRegistry::SetRequested(RenderEngineType type) {
		if (type == s_Requested) return;
		IRenderPipeline* before = GetActive();
		s_Requested = type;
		IRenderPipeline* after = GetActive();
		if (before != after) {
			before->OnDeactivate();
			after->OnActivate();
		}
		if (!Get(type)->IsAvailable())
			LNX_LOG_WARN("Render engine '{0}' is not available yet; using '{1}'", Get(type)->GetName(), after->GetName());
	}

	RenderEngineType RenderPipelineRegistry::GetRequested() { return s_Requested; }

	IRenderPipeline* RenderPipelineRegistry::GetActive() {
		IRenderPipeline* p = Get(s_Requested);
		return p->IsAvailable() ? p : &s_Raster;
	}

} // namespace Lunex
