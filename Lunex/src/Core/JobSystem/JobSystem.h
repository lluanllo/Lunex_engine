#pragma once

#include "Core/Core.h"
#include "Core/Timestep.h"
#include "Job.h"
#include "JobCounter.h"
#include "CommandBuffer.h"
#include "WorkStealingDeque.h"

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <functional>

namespace Lunex {

	/// <summary>
	/// Production-grade work-stealing job scheduler for Lunex Engine.
	/// 
	/// Architecture:
	/// - (N-1) worker threads with lock-free work-stealing deques
	/// - 1 main thread (game loop)
	/// - 2 IO worker threads (async file operations)
	/// - Global high-priority queue (mutex-protected)
	/// - Per-thread command buffers for main-thread operations
	/// 
	/// Scheduling Strategy:
	/// - High/Critical priority: Global queue (FIFO)
	/// - Normal/Low priority: Local deques (LIFO for owner, FIFO for thieves)
	/// - Worker loop: Try local ? Try global ? Try steal ? Yield
	/// 
	/// Thread Safety:
	/// - All public methods are thread-safe
	/// - Work-stealing deques are lock-free for owner operations
	/// - Command buffers use thread-local storage + mutex merge
	/// 
	/// Scene Versioning:
	/// - All jobs and commands store scene version
	/// - CancelByToken() marks outdated work for skipping
	/// - FlushMainThreadCommands() filters by version
	/// 
	/// Usage Example:
	/// ```cpp
	/// // Initialization (EditorLayer::OnAttach)
	/// JobSystemConfig config;
	/// config.NumWorkers = std::thread::hardware_concurrency() - 1;
	/// JobSystem::Init(config);
	/// 
	/// // Schedule work
	/// auto counter = JobSystem::Get().CreateCounter(1);
	/// JobSystem::Get().Schedule(
	///     []() { /* do work */ },
	///     nullptr,
	///     counter,
	///     JobPriority::Normal
	/// );
	/// 
	/// // Wait (safe pattern for main thread)
	/// while (!counter->Poll()) {
	///     JobSystem::Get().FlushMainThreadCommands(sceneVersion);
	///     std::this_thread::yield();
	/// }
	/// 
	/// // Shutdown (EditorLayer::OnDetach)
	/// JobSystem::Get().WaitForAllJobs();
	/// JobSystem::Shutdown();
	/// ```
	/// </summary>
	class JobSystem {
	public:
		// Constructor needs to be public for CreateScope
		JobSystem(const JobSystemConfig& config);
		~JobSystem();

		// Non-copyable, non-movable (singleton)
		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		/// <summary>
		/// Initialize job system with configuration.
		/// Call once at engine startup.
		/// </summary>
		/// <param name="config">Configuration parameters</param>
		static void Init(const JobSystemConfig& config = JobSystemConfig());

		/// <summary>
		/// Shutdown job system and wait for all workers to finish.
		/// Call once at engine shutdown.
		/// </summary>
		static void Shutdown();

		/// <summary>
		/// Get singleton instance (must call Init first).
		/// </summary>
		/// <returns>JobSystem instance</returns>
		static JobSystem& Get();

		// ====================================================================
		// JOB SCHEDULING
		// ====================================================================

		/// <summary>
		/// Schedule a job for execution.
		/// Thread-safe, can be called from any thread.
		/// </summary>
		/// <param name="func">Function to execute</param>
		/// <param name="userData">Optional user data pointer (not owned)</param>
		/// <param name="counter">Optional counter to decrement on completion</param>
		/// <param name="priority">Priority level (affects scheduling order)</param>
		/// <param name="sceneVersion">Scene version for cancellation (0 = never cancel)</param>
		/// <returns>Unique job handle</returns>
		JobHandle Schedule(
			JobFunc func,
			void* userData = nullptr,
			Ref<JobCounter> counter = nullptr,
			JobPriority priority = JobPriority::Normal,
			uint64_t sceneVersion = 0
		);

		/// <summary>
		/// Execute function in parallel over range [start, end).
		/// Automatically splits work into chunks and schedules jobs.
		/// </summary>
		/// <param name="start">Start index (inclusive)</param>
		/// <param name="end">End index (exclusive)</param>
		/// <param name="func">Function to execute for each index</param>
		/// <param name="grainSize">Chunk size (0 = auto-calculate)</param>
		/// <param name="priority">Priority level</param>
		/// <param name="sceneVersion">Scene version for cancellation</param>
		/// <returns>Counter tracking all jobs (use Wait or Poll)</returns>
		Ref<JobCounter> ParallelFor(
			uint32_t start,
			uint32_t end,
			std::function<void(uint32_t)> func,
			uint32_t grainSize = 0,
			JobPriority priority = JobPriority::Normal,
			uint64_t sceneVersion = 0
		);

		// ====================================================================
		// JOB COUNTER MANAGEMENT
		// ====================================================================

		/// <summary>
		/// Create a reference-counted job counter.
		/// </summary>
		/// <param name="initialValue">Starting count</param>
		/// <returns>Shared pointer to counter</returns>
		Ref<JobCounter> CreateCounter(int32_t initialValue = 0);

		/// <summary>
		/// Wait for counter to reach zero (blocking).
		/// 
		/// WARNING: Do NOT call on main thread if jobs enqueue main-thread commands!
		/// Use Poll + FlushMainThreadCommands pattern instead.
		/// </summary>
		/// <param name="counter">Counter to wait for</param>
		void Wait(Ref<JobCounter> counter);

		/// <summary>
		/// Poll counter without blocking (safe for main thread).
		/// </summary>
		/// <param name="counter">Counter to check</param>
		/// <returns>True if counter is zero</returns>
		bool Poll(Ref<JobCounter> counter);

		// ====================================================================
		// MAIN-THREAD COMMANDS
		// ====================================================================

		/// <summary>
		/// Push command to main-thread queue (thread-safe).
		/// Command will be executed on next FlushMainThreadCommands call.
		/// </summary>
		/// <param name="cmd">Command to execute (moved)</param>
		void PushMainThreadCommand(Command&& cmd);

		/// <summary>
		/// Execute all pending main-thread commands for given scene version.
		/// Call this from main thread (game loop) every frame.
		/// Commands with different scene version are discarded (cancelled).
		/// </summary>
		/// <param name="sceneVersion">Current scene version</param>
		void FlushMainThreadCommands(uint64_t sceneVersion);

		// ====================================================================
		// CANCELLATION
		// ====================================================================

		/// <summary>
		/// Cancel all jobs/commands matching given scene version.
		/// Jobs already running will complete, but won't be rescheduled.
		/// </summary>
		/// <param name="sceneVersion">Scene version to cancel</param>
		void CancelByToken(uint64_t sceneVersion);

		// ====================================================================
		// SYNCHRONIZATION
		// ====================================================================

		/// <summary>
		/// Wait for all pending jobs to complete (blocking).
		/// Use for engine shutdown or scene transitions.
		/// </summary>
		void WaitForAllJobs();

		// ====================================================================
		// METRICS & PROFILING
		// ====================================================================

		/// <summary>
		/// Get current performance metrics (atomic reads).
		/// </summary>
		/// <returns>Snapshot of current metrics</returns>
		JobMetricsSnapshot GetMetrics() const;

		/// <summary>
		/// Reset all metrics counters to zero.
		/// </summary>
		void ResetMetrics();

		/// <summary>
		/// Update metrics (called internally by workers).
		/// </summary>
		void UpdateMetrics();

	private:
		// ====================================================================
		// WORKER THREAD
		// ====================================================================

		/// <summary>
		/// Worker thread data (one per worker).
		/// </summary>
		struct WorkerThread {
			std::thread Thread;
			WorkStealingDeque<Job> Deque;
			uint32_t WorkerID;
			std::atomic<bool> IsActive{ false };

			explicit WorkerThread(uint32_t id, size_t dequeCapacity)
				: Deque(dequeCapacity), WorkerID(id) {}
		};

		/// <summary>
		/// Main worker loop (runs on worker threads).
		/// </summary>
		/// <param name="workerID">Index of this worker</param>
		void WorkerLoop(uint32_t workerID);

		/// <summary>
		/// Try to get next job for worker (local ? global ? steal).
		/// </summary>
		/// <param name="workerID">Index of this worker</param>
		/// <param name="outJob">Output job if found</param>
		/// <returns>True if job found</returns>
		bool TryGetJob(uint32_t workerID, Job& outJob);

		/// <summary>
		/// Execute job and handle counter/metrics.
		/// </summary>
		/// <param name="job">Job to execute</param>
		void ExecuteJob(Job& job);

		// ====================================================================
		// IO WORKER THREAD
		// ====================================================================

		/// <summary>
		/// IO worker loop (runs on IO threads).
		/// </summary>
		/// <param name="workerID">Index of this IO worker</param>
		void IOWorkerLoop(uint32_t workerID);

		// ====================================================================
		// INTERNAL STATE
		// ====================================================================

		// Configuration
		JobSystemConfig m_Config;

		// Worker threads
		std::vector<Scope<WorkerThread>> m_Workers;
		std::vector<std::thread> m_IOWorkers;

		// Global high-priority queue (mutex-protected)
		std::queue<Job> m_GlobalQueue;
		std::mutex m_GlobalQueueMutex;
		std::condition_variable m_GlobalQueueCV;

		// IO job queue
		std::queue<Job> m_IOQueue;
		std::mutex m_IOQueueMutex;
		std::condition_variable m_IOQueueCV;

		// Main-thread command buffer (mutex-protected)
		std::vector<Command> m_MainThreadCommands;
		std::mutex m_MainThreadMutex;

		// Thread-local command buffers (per worker)
		std::unordered_map<std::thread::id, CommandBuffer> m_ThreadLocalCommands;
		std::mutex m_ThreadLocalMutex;

		// Cancellation tokens
		std::unordered_set<uint64_t> m_CancelledVersions;
		std::mutex m_CancellationMutex;

		// Runtime state
		std::atomic<bool> m_Running{ false };
		std::atomic<uint64_t> m_NextJobHandle{ 1 };

		// Metrics
		JobMetrics m_Metrics;

		// Main thread context (set once at init)
		MainThreadContext m_MainThreadContext;

		// Singleton instance
		static Scope<JobSystem> s_Instance;
	};

} // namespace Lunex
