#include "stpch.h"
#include "JobSystem.h"
#include "Log/Log.h"
#include <chrono>
#include <algorithm>

namespace Lunex {

	// Singleton instance
	Scope<JobSystem> JobSystem::s_Instance = nullptr;

	// ========================================================================
	// INITIALIZATION & SHUTDOWN
	// ========================================================================

	void JobSystem::Init(const JobSystemConfig& config) {
		LNX_CORE_ASSERT(!s_Instance, "JobSystem already initialized!");
		s_Instance = CreateScope<JobSystem>(config);
		LNX_LOG_INFO("JobSystem initialized with {0} workers and {1} IO workers",
			s_Instance->m_Workers.size(), config.NumIOWorkers);
	}

	void JobSystem::Shutdown() {
		LNX_CORE_ASSERT(s_Instance, "JobSystem not initialized!");
		s_Instance.reset();
		LNX_LOG_INFO("JobSystem shut down");
	}

	JobSystem& JobSystem::Get() {
		LNX_CORE_ASSERT(s_Instance, "JobSystem not initialized! Call JobSystem::Init() first");
		return *s_Instance;
	}

	JobSystem::JobSystem(const JobSystemConfig& config)
		: m_Config(config) {
		
		// Auto-detect worker count if not specified
		if (m_Config.NumWorkers == 0) {
			uint32_t hwThreads = std::thread::hardware_concurrency();
			m_Config.NumWorkers = std::max(1u, hwThreads - 1); // Reserve 1 for main thread
		}

		// Create worker threads
		m_Workers.reserve(m_Config.NumWorkers);
		for (uint32_t i = 0; i < m_Config.NumWorkers; ++i) {
			m_Workers.push_back(CreateScope<WorkerThread>(i, m_Config.WorkerDequeCapacity));
		}

		// Start system
		m_Running.store(true, std::memory_order_release);

		// Launch worker threads
		for (uint32_t i = 0; i < m_Config.NumWorkers; ++i) {
			m_Workers[i]->Thread = std::thread(&JobSystem::WorkerLoop, this, i);
		}

		// Launch IO workers
		for (uint32_t i = 0; i < m_Config.NumIOWorkers; ++i) {
			m_IOWorkers.emplace_back(&JobSystem::IOWorkerLoop, this, i);
		}

		LNX_LOG_INFO("JobSystem: {0} worker threads + {1} IO threads started",
			m_Config.NumWorkers, m_Config.NumIOWorkers);
	}

	JobSystem::~JobSystem() {
		// Signal shutdown
		m_Running.store(false, std::memory_order_release);

		// Wake up all workers
		m_GlobalQueueCV.notify_all();
		m_IOQueueCV.notify_all();

		// Join worker threads
		for (auto& worker : m_Workers) {
			if (worker->Thread.joinable()) {
				worker->Thread.join();
			}
		}

		// Join IO workers
		for (auto& thread : m_IOWorkers) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		LNX_LOG_INFO("JobSystem: All worker threads stopped");
	}

	// ========================================================================
	// JOB SCHEDULING
	// ========================================================================

	JobHandle JobSystem::Schedule(
		JobFunc func,
		void* userData,
		Ref<JobCounter> counter,
		JobPriority priority,
		uint64_t sceneVersion
	) {
		LNX_CORE_ASSERT(func, "Job function cannot be null!");

		// Create job
		Job job;
		job.Function = std::move(func);
		job.UserData = userData;
		job.Counter = counter;
		job.Priority = priority;
		job.SceneVersion = sceneVersion;
		job.Handle = m_NextJobHandle.fetch_add(1, std::memory_order_relaxed);

		// Update metrics
		m_Metrics.TotalJobsScheduled.fetch_add(1, std::memory_order_relaxed);

		// Schedule based on priority
		if (priority == JobPriority::High || priority == JobPriority::Critical) {
			// High-priority jobs go to global queue
			{
				std::scoped_lock lock(m_GlobalQueueMutex);
				m_GlobalQueue.push(std::move(job));
				m_Metrics.GlobalQueueSize.fetch_add(1, std::memory_order_relaxed);
			}
			m_GlobalQueueCV.notify_one();
		}
		else {
			// Normal/Low priority: Try to push to calling thread's deque
			std::thread::id tid = std::this_thread::get_id();

			// Find worker thread that owns this thread ID
			uint32_t workerID = 0;
			bool foundWorker = false;

			for (uint32_t i = 0; i < m_Workers.size(); ++i) {
				if (m_Workers[i]->Thread.get_id() == tid) {
					workerID = i;
					foundWorker = true;
					break;
				}
			}

			if (foundWorker) {
				// Push to local deque (lock-free)
				m_Workers[workerID]->Deque.Push(std::move(job));
			}
			else {
				// Called from non-worker thread (e.g., main thread)
				// Push to global queue
				{
					std::scoped_lock lock(m_GlobalQueueMutex);
					m_GlobalQueue.push(std::move(job));
					m_Metrics.GlobalQueueSize.fetch_add(1, std::memory_order_relaxed);
				}
				m_GlobalQueueCV.notify_one();
			}
		}

		return job.Handle;
	}

	// ========================================================================
	// WORKER LOOP
	// ========================================================================

	void JobSystem::WorkerLoop(uint32_t workerID) {
		LNX_LOG_TRACE("Worker {0} started", workerID);
		m_Workers[workerID]->IsActive.store(true, std::memory_order_release);

		while (m_Running.load(std::memory_order_acquire)) {
			Job job;

			// Try to get job (local ? global ? steal)
			if (TryGetJob(workerID, job)) {
				// Execute job
				ExecuteJob(job);
			}
			else {
				// No work available, yield CPU
				std::this_thread::yield();

				// If still no work, wait on condition variable
				if (m_Config.EnableWorkStealing) {
					std::unique_lock lock(m_GlobalQueueMutex);
					m_GlobalQueueCV.wait_for(lock, std::chrono::milliseconds(1), [this]() {
						return !m_Running.load(std::memory_order_acquire) ||
							!m_GlobalQueue.empty();
					});
				}
			}
		}

		m_Workers[workerID]->IsActive.store(false, std::memory_order_release);
		LNX_LOG_TRACE("Worker {0} stopped", workerID);
	}

	bool JobSystem::TryGetJob(uint32_t workerID, Job& outJob) {
		// 1. Try local deque (LIFO, cache-friendly)
		{
			auto job = m_Workers[workerID]->Deque.Pop();
			if (job.has_value()) {
				outJob = std::move(job.value());
				return true;
			}
		}

		// 2. Try global queue (high-priority jobs)
		{
			std::scoped_lock lock(m_GlobalQueueMutex);
			if (!m_GlobalQueue.empty()) {
				outJob = std::move(m_GlobalQueue.front());
				m_GlobalQueue.pop();
				m_Metrics.GlobalQueueSize.fetch_sub(1, std::memory_order_relaxed);
				return true;
			}
		}

		// 3. Try work-stealing (FIFO from other workers)
		if (m_Config.EnableWorkStealing) {
			// Random starting point to distribute load
			uint32_t startOffset = workerID + 1;

			for (uint32_t i = 0; i < m_Workers.size() - 1; ++i) {
				uint32_t victimID = (startOffset + i) % m_Workers.size();

				if (victimID == workerID) continue;

				auto job = m_Workers[victimID]->Deque.Steal();
				if (job.has_value()) {
					outJob = std::move(job.value());
					m_Metrics.TotalJobsStolen.fetch_add(1, std::memory_order_relaxed);
					return true;
				}
			}
		}

		return false;
	}

	void JobSystem::ExecuteJob(Job& job) {
		// Check if job is cancelled
		{
			std::scoped_lock lock(m_CancellationMutex);
			if (m_CancelledVersions.find(job.SceneVersion) != m_CancelledVersions.end()) {
				// Job is cancelled, skip execution
				if (job.Counter) {
					job.Counter->Decrement();
				}
				return;
			}
		}

		// Execute job function
		try {
			if (job.Function) {
				job.Function();
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Job execution failed: {0}", e.what());
		}
		catch (...) {
			LNX_LOG_ERROR("Job execution failed with unknown exception");
		}

		// Decrement counter if present
		if (job.Counter) {
			job.Counter->Decrement();
		}

		// Update metrics
		m_Metrics.TotalJobsCompleted.fetch_add(1, std::memory_order_relaxed);
	}

	// ========================================================================
	// IO WORKER LOOP
	// ========================================================================

	void JobSystem::IOWorkerLoop(uint32_t workerID) {
		LNX_LOG_TRACE("IO Worker {0} started", workerID);

		while (m_Running.load(std::memory_order_acquire)) {
			Job job;

			// Try to get IO job
			{
				std::unique_lock lock(m_IOQueueMutex);
				m_IOQueueCV.wait_for(lock, std::chrono::milliseconds(10), [this]() {
					return !m_Running.load(std::memory_order_acquire) ||
						!m_IOQueue.empty();
				});

				if (!m_IOQueue.empty()) {
					job = std::move(m_IOQueue.front());
					m_IOQueue.pop();
				}
				else {
					continue;
				}
			}

			// Execute IO job
			ExecuteJob(job);
		}

		LNX_LOG_TRACE("IO Worker {0} stopped", workerID);
	}

	// ========================================================================
	// PARALLEL FOR
	// ========================================================================

	Ref<JobCounter> JobSystem::ParallelFor(
		uint32_t start,
		uint32_t end,
		std::function<void(uint32_t)> func,
		uint32_t grainSize,
		JobPriority priority,
		uint64_t sceneVersion
	) {
		LNX_CORE_ASSERT(start <= end, "Invalid range: start > end");
		LNX_CORE_ASSERT(func, "ParallelFor function cannot be null!");

		if (start == end) {
			return CreateCounter(0); // Empty range
		}

		// Auto-calculate grain size
		if (grainSize == 0) {
			uint32_t numElements = end - start;
			uint32_t numWorkers = static_cast<uint32_t>(m_Workers.size());
			grainSize = std::max(1u, numElements / (numWorkers * 4));
		}

		// Calculate number of jobs
		uint32_t numElements = end - start;
		uint32_t numJobs = (numElements + grainSize - 1) / grainSize;

		// Create counter for all jobs
		auto counter = CreateCounter(static_cast<int32_t>(numJobs));

		// Store function in shared_ptr to avoid copy issues with std::function in lambdas
		auto sharedFunc = std::make_shared<std::function<void(uint32_t)>>(std::move(func));

		// Schedule jobs
		for (uint32_t jobIndex = 0; jobIndex < numJobs; ++jobIndex) {
			uint32_t jobStart = start + jobIndex * grainSize;
			uint32_t jobEnd = std::min(jobStart + grainSize, end);

			Schedule(
				[sharedFunc, jobStart, jobEnd]() {
					for (uint32_t i = jobStart; i < jobEnd; ++i) {
						(*sharedFunc)(i);
					}
				},
				nullptr,
				counter, // Pass counter to Schedule so it gets decremented automatically
				priority,
				sceneVersion
			);
		}

		return counter;
	}

	// ========================================================================
	// JOB COUNTER MANAGEMENT
	// ========================================================================

	Ref<JobCounter> JobSystem::CreateCounter(int32_t initialValue) {
		return CreateRef<JobCounter>(initialValue);
	}

	void JobSystem::Wait(Ref<JobCounter> counter) {
		LNX_CORE_ASSERT(counter, "Counter cannot be null!");
		counter->Wait();
	}

	bool JobSystem::Poll(Ref<JobCounter> counter) {
		LNX_CORE_ASSERT(counter, "Counter cannot be null!");
		return counter->Poll();
	}

	// ========================================================================
	// MAIN-THREAD COMMANDS
	// ========================================================================

	void JobSystem::PushMainThreadCommand(Command&& cmd) {
		std::scoped_lock lock(m_MainThreadMutex);
		m_MainThreadCommands.push_back(std::move(cmd));
		m_Metrics.CommandBufferSize.fetch_add(1, std::memory_order_relaxed);
	}

	void JobSystem::FlushMainThreadCommands(uint64_t sceneVersion) {
		std::vector<Command> commands;

		// Extract commands
		{
			std::scoped_lock lock(m_MainThreadMutex);
			commands = std::move(m_MainThreadCommands);
			m_MainThreadCommands.clear();
		}

		// Execute commands matching scene version
		uint32_t executed = 0;
		uint32_t cancelled = 0;

		for (auto& cmd : commands) {
			if (cmd.SceneVersion == 0 || cmd.SceneVersion == sceneVersion) {
				try {
					if (cmd.Function) {
						cmd.Function(m_MainThreadContext);
						executed++;
					}
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("Main-thread command failed: {0}", e.what());
				}
				catch (...) {
					LNX_LOG_ERROR("Main-thread command failed with unknown exception");
				}
			}
			else {
				// Command is for different scene version, cancelled
				cancelled++;
			}
		}

		// Update metrics
		m_Metrics.CommandBufferSize.store(0, std::memory_order_relaxed);

		if (cancelled > 0) {
			LNX_LOG_TRACE("FlushMainThreadCommands: {0} executed, {1} cancelled", executed, cancelled);
		}
	}

	// ========================================================================
	// CANCELLATION
	// ========================================================================

	void JobSystem::CancelByToken(uint64_t sceneVersion) {
		std::scoped_lock lock(m_CancellationMutex);
		m_CancelledVersions.insert(sceneVersion);
		LNX_LOG_INFO("Cancelled all jobs/commands for scene version {0}", sceneVersion);
	}

	// ========================================================================
	// SYNCHRONIZATION
	// ========================================================================

	void JobSystem::WaitForAllJobs() {
		LNX_LOG_INFO("Waiting for all jobs to complete...");

		// Wait until all worker deques and global queue are empty
		while (true) {
			bool allEmpty = true;

			// Check global queue
			{
				std::scoped_lock lock(m_GlobalQueueMutex);
				if (!m_GlobalQueue.empty()) {
					allEmpty = false;
				}
			}

			// Check worker deques
			for (auto& worker : m_Workers) {
				if (!worker->Deque.IsEmpty()) {
					allEmpty = false;
					break;
				}
			}

			if (allEmpty) {
				break;
			}

			std::this_thread::yield();
		}

		LNX_LOG_INFO("All jobs completed");
	}

	// ========================================================================
	// METRICS & PROFILING
	// ========================================================================

	JobMetricsSnapshot JobSystem::GetMetrics() const {
		// Create snapshot of metrics (all atomics read with relaxed ordering)
		JobMetricsSnapshot snapshot;
		
		snapshot.TotalJobsScheduled = m_Metrics.TotalJobsScheduled.load(std::memory_order_relaxed);
		snapshot.TotalJobsCompleted = m_Metrics.TotalJobsCompleted.load(std::memory_order_relaxed);
		snapshot.TotalJobsStolen = m_Metrics.TotalJobsStolen.load(std::memory_order_relaxed);
		snapshot.GlobalQueueSize = m_Metrics.GlobalQueueSize.load(std::memory_order_relaxed);
		snapshot.CommandBufferSize = m_Metrics.CommandBufferSize.load(std::memory_order_relaxed);
		snapshot.AvgJobLatencyMs = m_Metrics.AvgJobLatencyMs.load(std::memory_order_relaxed);
		snapshot.Throughput = m_Metrics.Throughput.load(std::memory_order_relaxed);
		
		// Copy worker queue sizes
		for (size_t i = 0; i < m_Workers.size() && i < snapshot.WorkerQueueSizes.size(); ++i) {
			uint32_t size = static_cast<uint32_t>(m_Workers[i]->Deque.Size());
			snapshot.WorkerQueueSizes[i] = size;
		}

		// Count active workers
		uint32_t active = 0;
		uint32_t idle = 0;
		for (const auto& worker : m_Workers) {
			if (worker->IsActive.load(std::memory_order_relaxed)) {
				active++;
			}
			else {
				idle++;
			}
		}
		snapshot.ActiveWorkers = active;
		snapshot.IdleWorkers = idle;

		return snapshot;
	}

	void JobSystem::ResetMetrics() {
		m_Metrics.TotalJobsScheduled.store(0, std::memory_order_relaxed);
		m_Metrics.TotalJobsCompleted.store(0, std::memory_order_relaxed);
		m_Metrics.TotalJobsStolen.store(0, std::memory_order_relaxed);
		m_Metrics.GlobalQueueSize.store(0, std::memory_order_relaxed);
		m_Metrics.CommandBufferSize.store(0, std::memory_order_relaxed);
		m_Metrics.AvgJobLatencyMs.store(0.0f, std::memory_order_relaxed);
		m_Metrics.Throughput.store(0.0f, std::memory_order_relaxed);

		for (auto& size : m_Metrics.WorkerQueueSizes) {
			size.store(0, std::memory_order_relaxed);
		}

		LNX_LOG_INFO("JobSystem metrics reset");
	}

	void JobSystem::UpdateMetrics() {
		// Calculate throughput (jobs/second)
		static auto lastUpdate = std::chrono::steady_clock::now();
		static uint64_t lastCompleted = 0;

		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

		if (elapsed >= 1000) { // Update every second
			uint64_t completed = m_Metrics.TotalJobsCompleted.load(std::memory_order_relaxed);
			uint64_t delta = completed - lastCompleted;

			float throughput = static_cast<float>(delta) / (elapsed / 1000.0f);
			m_Metrics.Throughput.store(throughput, std::memory_order_relaxed);

			lastUpdate = now;
			lastCompleted = completed;
		}
	}

} // namespace Lunex
