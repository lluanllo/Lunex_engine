#pragma once

/**
 * @file FrameProfiler.h
 * @brief Per-frame engine metrics for the Stats panel: what costs what, every frame.
 *
 *  - Named sections with CPU time and (optionally) GPU time via GL timer queries.
 *    GPU sections must not overlap each other (GL_TIME_ELAPSED cannot nest); CPU-only
 *    sections may nest freely. GPU results arrive a few frames late (no stalls).
 *  - Frame time history, FPS, 1% low.
 *  - Render counters snapshot of the main view (previews render after it and would
 *    otherwise reset/pollute the renderer statistics).
 *  - Process RAM and driver-reported VRAM.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace Lunex {

	class FrameProfiler {
	public:
		struct Section {
			std::string Name;
			float CpuMs = 0.0f;   // smoothed
			float GpuMs = -1.0f;  // smoothed, -1 = not measured on the GPU
			int Depth = 0;
		};

		struct RenderCounters {
			uint32_t GeometryDrawCalls = 0;
			uint32_t LightingDrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t CulledMeshes = 0;
			uint64_t Triangles = 0;
			uint32_t DrawCalls2D = 0;
			uint32_t Quads2D = 0;
		};

		// Frame boundaries (frameMs = last frame's duration)
		static void BeginFrame(float frameMs);

		static void Begin(const char* name, bool gpu = false);
		static void End();
		// GPU time measured outside GL queries (e.g. Vulkan timestamps) for a section recorded this frame
		static void ReportGpuTime(const char* name, float ms);

		static const std::vector<Section>& GetSections() { return s_Results; }
		static const std::vector<float>& GetFrameTimes() { return s_FrameTimes; }   // oldest first
		static float GetAverageMs();
		static float GetMaxMs();
		static float GetOnePercentLowFps();

		static void SetRenderCounters(const RenderCounters& c) { s_Counters = c; }
		static const RenderCounters& GetRenderCounters() { return s_Counters; }

		// Memory
		static uint64_t GetProcessWorkingSet();   // bytes
		static uint64_t GetProcessPrivateBytes(); // bytes
		static bool GetVideoMemory(uint64_t& totalBytes, uint64_t& availableBytes);
		static const char* GetGpuName();
		static void GetTextureStats(uint32_t& count, uint64_t& bytes);   // live 2D textures

	private:
		static std::vector<Section> s_Results;
		static std::vector<float> s_FrameTimes;
		static RenderCounters s_Counters;
	};

	struct ProfileScope {
		ProfileScope(const char* name, bool gpu = false) { FrameProfiler::Begin(name, gpu); }
		~ProfileScope() { FrameProfiler::End(); }
		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;
	};

} // namespace Lunex
