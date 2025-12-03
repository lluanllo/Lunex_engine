#pragma once

#include "Core/Core.h"
#include "JobCounter.h"
#include <functional>
#include <cstdint>
#include <array>

namespace Lunex {

	/// <summary>
	/// Job priority levels for scheduling.
	/// Higher priority jobs are executed first from global queue.
	/// </summary>
	enum class JobPriority {
		Low = 0,      // Background tasks (asset streaming, etc.)
		Normal = 1,   // Default priority
		High = 2,     // Time-critical work (physics, animation)
		Critical = 3  // Must execute immediately (user-facing tasks)
	};

	/// <summary>
	/// Unique handle for tracking scheduled jobs.
	/// Can be used for cancellation or status queries.
	/// </summary>
	using JobHandle = uint64_t;

	/// <summary>
	/// Job function signature (no parameters, no return value).
	/// Capture state via lambda or bind.
	/// </summary>
	using JobFunc = std::function<void()>;

	/// <summary>
	/// Core job structure for work-stealing scheduler.
	/// 
	/// Lifetime: Job objects are moved into deques, executed once, then destroyed.
	/// All captured state must remain valid until job executes.
	/// 
	/// Usage Example:
	/// ```cpp
	/// Job job;
	/// job.Function = []() { /* work */ };
	/// job.Priority = JobPriority::High;
	/// job.Counter = counter; // Decremented when job completes
	/// job.SceneVersion = scene->GetVersion(); // For cancellation
	/// ```
	/// </summary>
	struct Job {
		/// Function to execute (may capture state)
		JobFunc Function;

		/// Optional user data pointer (NOT owned, must outlive job)
		void* UserData = nullptr;

		/// Optional counter to decrement on completion
		Ref<JobCounter> Counter;

		/// Priority level (affects scheduling order)
		JobPriority Priority = JobPriority::Normal;

		/// Scene version for cancellation (0 = no cancellation)
		uint64_t SceneVersion = 0;

		/// Unique handle for this job (assigned by scheduler)
		JobHandle Handle = 0;

		/// Timestamp when job was created (for latency tracking)
		float CreationTime = 0.0f;
	};

	/// <summary>
	/// Configuration for JobSystem initialization.
	/// </summary>
	struct JobSystemConfig {
		/// Number of worker threads (default: hardware_concurrency - 1)
		uint32_t NumWorkers = 0; // 0 = auto-detect

		/// Number of IO worker threads for async file operations
		uint32_t NumIOWorkers = 2;

		/// Enable work-stealing (disable for debugging)
		bool EnableWorkStealing = true;

		/// Enable profiling markers and metrics
		bool EnableProfiling = true;

		/// Maximum capacity for global high-priority queue
		uint32_t GlobalQueueCapacity = 1024;

		/// Initial capacity for per-worker deques
		uint32_t WorkerDequeCapacity = 512;
	};

	/// <summary>
	/// Snapshot of metrics (non-atomic, safe to copy).
	/// Returned by JobSystem::GetMetrics().
	/// </summary>
	struct JobMetricsSnapshot {
		uint64_t TotalJobsScheduled = 0;
		uint64_t TotalJobsCompleted = 0;
		uint64_t TotalJobsStolen = 0;
		uint32_t GlobalQueueSize = 0;
		uint32_t CommandBufferSize = 0;
		std::array<uint32_t, 16> WorkerQueueSizes{};
		float AvgJobLatencyMs = 0.0f;
		float Throughput = 0.0f;
		uint32_t ActiveWorkers = 0;
		uint32_t IdleWorkers = 0;
	};

	/// <summary>
	/// Runtime metrics for JobSystem monitoring.
	/// All counters are atomic for lock-free updates.
	/// </summary>
	struct JobMetrics {
		/// Total jobs scheduled since initialization
		std::atomic<uint64_t> TotalJobsScheduled{ 0 };

		/// Total jobs completed successfully
		std::atomic<uint64_t> TotalJobsCompleted{ 0 };

		/// Total jobs stolen by work-stealing
		std::atomic<uint64_t> TotalJobsStolen{ 0 };

		/// Current size of global high-priority queue
		std::atomic<uint32_t> GlobalQueueSize{ 0 };

		/// Current size of main-thread command buffer
		std::atomic<uint32_t> CommandBufferSize{ 0 };

		/// Per-worker queue sizes (approximate)
		std::array<std::atomic<uint32_t>, 16> WorkerQueueSizes;

		/// Average job execution latency (milliseconds)
		std::atomic<float> AvgJobLatencyMs{ 0.0f };

		/// Jobs completed per second (throughput)
		std::atomic<float> Throughput{ 0.0f };

		/// Number of active worker threads
		std::atomic<uint32_t> ActiveWorkers{ 0 };

		/// Number of idle worker threads
		std::atomic<uint32_t> IdleWorkers{ 0 };

		JobMetrics() {
			for (auto& size : WorkerQueueSizes) {
				size.store(0, std::memory_order_relaxed);
			}
		}
	};

} // namespace Lunex
