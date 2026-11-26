#include "stpch.h"
#include "FrameProfiler.h"

#include <glad/glad.h>
#include "RHI/OpenGL/OpenGLRHITexture.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Lunex {

	std::vector<FrameProfiler::Section> FrameProfiler::s_Results;
	std::vector<float> FrameProfiler::s_FrameTimes;
	FrameProfiler::RenderCounters FrameProfiler::s_Counters;

	namespace {
		using Clock = std::chrono::steady_clock;
		constexpr int kSlots = 4;          // frames in flight for GPU queries
		constexpr size_t kHistory = 240;

		struct Recorded {
			std::string Name;
			int Depth = 0;
			float CpuMs = 0.0f;
			GLuint Query = 0;          // 0 = no GPU timing
		};

		struct Open {
			size_t Index;
			Clock::time_point Start;
		};

		std::vector<Recorded> g_Slots[kSlots];
		std::vector<GLuint> g_FreeQueries;
		std::vector<Open> g_Stack;
		int g_Frame = 0;
		bool g_GpuOpen = false;
		bool g_GpuSupported = true;

		struct Smoothed { float Cpu = 0.0f; float Gpu = -1.0f; int Depth = 0; int Order = 0; int LastSeen = 0; };
		std::unordered_map<std::string, Smoothed> g_Smoothed;

		GLuint AcquireQuery() {
			if (!g_FreeQueries.empty()) { GLuint q = g_FreeQueries.back(); g_FreeQueries.pop_back(); return q; }
			GLuint q = 0;
			glGenQueries(1, &q);
			return q;
		}

		float Smooth(float current, float sample) { return current <= 0.0f ? sample : current * 0.9f + sample * 0.1f; }
	}

	void FrameProfiler::BeginFrame(float frameMs) {
		// Close anything left open (exceptions / early returns)
		while (!g_Stack.empty()) End();

		s_FrameTimes.push_back(frameMs);
		if (s_FrameTimes.size() > kHistory) s_FrameTimes.erase(s_FrameTimes.begin());

		g_Frame++;
		std::vector<Recorded>& slot = g_Slots[g_Frame % kSlots];

		// Resolve the frame recorded kSlots frames ago (its GPU queries are done by now)
		for (size_t i = 0; i < slot.size(); i++) {
			Recorded& r = slot[i];
			Smoothed& s = g_Smoothed[r.Name];
			s.Cpu = Smooth(s.Cpu, r.CpuMs);
			s.Depth = r.Depth;
			s.Order = static_cast<int>(i);
			s.LastSeen = g_Frame;
			if (r.Query) {
				GLuint64 ns = 0;
				GLint available = 0;
				glGetQueryObjectiv(r.Query, GL_QUERY_RESULT_AVAILABLE, &available);
				if (available) {
					glGetQueryObjectui64v(r.Query, GL_QUERY_RESULT, &ns);
					s.Gpu = Smooth(s.Gpu < 0.0f ? 0.0f : s.Gpu, static_cast<float>(ns) / 1.0e6f);
				}
				g_FreeQueries.push_back(r.Query);
			}
		}
		slot.clear();

		// Publish (sections seen recently, in recording order)
		s_Results.clear();
		std::vector<std::pair<int, Section>> ordered;
		for (auto& [name, s] : g_Smoothed) {
			if (g_Frame - s.LastSeen > 30) continue;
			ordered.push_back({ s.Order, Section{ name, s.Cpu, s.Gpu, s.Depth } });
		}
		std::sort(ordered.begin(), ordered.end(), [](auto& a, auto& b) { return a.first < b.first; });
		for (auto& o : ordered) s_Results.push_back(o.second);
	}

	void FrameProfiler::Begin(const char* name, bool gpu) {
		std::vector<Recorded>& slot = g_Slots[g_Frame % kSlots];
		Recorded r;
		r.Name = name;
		r.Depth = static_cast<int>(g_Stack.size());
		if (gpu && g_GpuSupported && !g_GpuOpen && glBeginQuery) {
			r.Query = AcquireQuery();
			glBeginQuery(GL_TIME_ELAPSED, r.Query);
			g_GpuOpen = true;
		}
		slot.push_back(r);
		g_Stack.push_back({ slot.size() - 1, Clock::now() });
	}

	void FrameProfiler::End() {
		if (g_Stack.empty()) return;
		Open open = g_Stack.back();
		g_Stack.pop_back();
		std::vector<Recorded>& slot = g_Slots[g_Frame % kSlots];
		if (open.Index >= slot.size()) return;
		Recorded& r = slot[open.Index];
		r.CpuMs = std::chrono::duration<float, std::milli>(Clock::now() - open.Start).count();
		if (r.Query && g_GpuOpen) {
			glEndQuery(GL_TIME_ELAPSED);
			g_GpuOpen = false;
		}
	}

	float FrameProfiler::GetAverageMs() {
		if (s_FrameTimes.empty()) return 0.0f;
		const size_t n = std::min<size_t>(s_FrameTimes.size(), 60);
		float sum = 0.0f;
		for (size_t i = s_FrameTimes.size() - n; i < s_FrameTimes.size(); i++) sum += s_FrameTimes[i];
		return sum / static_cast<float>(n);
	}

	float FrameProfiler::GetMaxMs() {
		float m = 0.0f;
		for (float t : s_FrameTimes) m = std::max(m, t);
		return m;
	}

	float FrameProfiler::GetOnePercentLowFps() {
		if (s_FrameTimes.size() < 10) return 0.0f;
		std::vector<float> sorted = s_FrameTimes;
		std::sort(sorted.begin(), sorted.end(), std::greater<float>());
		const size_t n = std::max<size_t>(1, sorted.size() / 100);
		float sum = 0.0f;
		for (size_t i = 0; i < n; i++) sum += sorted[i];
		const float worstMs = sum / static_cast<float>(n);
		return worstMs > 0.0f ? 1000.0f / worstMs : 0.0f;
	}

	uint64_t FrameProfiler::GetProcessWorkingSet() {
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS_EX pmc{};
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
			return pmc.WorkingSetSize;
#endif
		return 0;
	}

	uint64_t FrameProfiler::GetProcessPrivateBytes() {
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS_EX pmc{};
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
			return pmc.PrivateUsage;
#endif
		return 0;
	}

	bool FrameProfiler::GetVideoMemory(uint64_t& totalBytes, uint64_t& availableBytes) {
		totalBytes = availableBytes = 0;
		while (glGetError() != GL_NO_ERROR) {}
		// NVIDIA: GL_NVX_gpu_memory_info (values in KB)
		GLint total = 0, available = 0;
		glGetIntegerv(0x9048, &total);       // GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
		glGetIntegerv(0x9049, &available);   // GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
		if (glGetError() == GL_NO_ERROR && total > 0) {
			totalBytes = static_cast<uint64_t>(total) * 1024ull;
			availableBytes = static_cast<uint64_t>(available) * 1024ull;
			return true;
		}
		// AMD: GL_ATI_meminfo (free texture memory in KB, no total)
		GLint ati[4] = {};
		glGetIntegerv(0x87FC, ati);          // TEXTURE_FREE_MEMORY_ATI
		if (glGetError() == GL_NO_ERROR && ati[0] > 0) {
			availableBytes = static_cast<uint64_t>(ati[0]) * 1024ull;
			return true;
		}
		return false;
	}

	void FrameProfiler::GetTextureStats(uint32_t& count, uint64_t& bytes) {
		count = RHI::OpenGLRHITexture2D::GetLiveCount();
		bytes = RHI::OpenGLRHITexture2D::GetLiveBytes();
	}

	const char* FrameProfiler::GetGpuName() {
		static std::string name = [] {
			const GLubyte* r = glGetString(GL_RENDERER);
			return r ? std::string(reinterpret_cast<const char*>(r)) : std::string("Unknown GPU");
		}();
		return name.c_str();
	}

} // namespace Lunex
