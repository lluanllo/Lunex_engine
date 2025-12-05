#pragma once

#include "Core/Core.h"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Lunex {

	/// <summary>
	/// Thread-safe reference-counted counter for tracking job completion.
	/// 
	/// Used to synchronize job dependencies and wait for completion.
	/// Supports both blocking wait (Wait) and non-blocking poll (Poll).
	/// 
	/// Memory Model:
	/// - Value uses release/acquire for job completion visibility
	/// - Notifies all waiting threads when counter reaches zero
	/// 
	/// Usage Example:
	/// ```cpp
	/// auto counter = JobSystem::Get().CreateCounter(10);
	/// 
	/// // Schedule 10 jobs with this counter
	/// for (int i = 0; i < 10; ++i) {
	///     JobSystem::Get().Schedule([counter]() {
	///         // Do work...
	///         counter->Decrement();
	///     });
	/// }
	/// 
	/// // Wait for all jobs to complete
	/// counter->Wait();
	/// 
	/// // Or poll without blocking
	/// if (counter->Poll()) {
	///     // All jobs finished
	/// }
	/// ```
	/// 
	/// WARNING: Do NOT call Wait() on main thread if jobs need main-thread commands!
	/// Use Poll() + FlushMainThreadCommands() instead:
	/// ```cpp
	/// while (!counter->Poll()) {
	///     JobSystem::Get().FlushMainThreadCommands(sceneVersion);
	///     std::this_thread::yield();
	/// }
	/// ```
	/// </summary>
	class JobCounter {
	public:
		/// <summary>
		/// Creates a counter with initial value.
		/// </summary>
		/// <param name="initialValue">Starting count (typically number of jobs)</param>
		explicit JobCounter(int32_t initialValue = 0);

		~JobCounter() = default;

		// Non-copyable (use Ref<JobCounter> for shared ownership)
		JobCounter(const JobCounter&) = delete;
		JobCounter& operator=(const JobCounter&) = delete;

		/// <summary>
		/// Add to counter (use for job submission).
		/// Thread-safe with release semantics.
		/// </summary>
		/// <param name="value">Amount to add (can be negative)</param>
		void Add(int32_t value);

		/// <summary>
		/// Decrement counter by 1 (use when job completes).
		/// Thread-safe with release semantics.
		/// Notifies waiting threads if counter reaches zero.
		/// </summary>
		void Decrement();

		/// <summary>
		/// Get current value (approximate due to concurrent operations).
		/// </summary>
		/// <returns>Current counter value</returns>
		int32_t GetValue() const {
			return m_Value.load(std::memory_order_acquire);
		}

		/// <summary>
		/// Check if counter is zero (non-blocking).
		/// </summary>
		/// <returns>True if all jobs completed</returns>
		bool IsZero() const {
			return GetValue() <= 0;
		}

		/// <summary>
		/// Wait until counter reaches zero (blocking).
		/// 
		/// WARNING: Do NOT call on main thread if jobs enqueue main-thread commands!
		/// Use Poll() + FlushMainThreadCommands() pattern instead.
		/// </summary>
		void Wait();

		/// <summary>
		/// Poll counter without blocking (safe for main thread).
		/// </summary>
		/// <returns>True if counter is zero</returns>
		bool Poll() const {
			return IsZero();
		}

		/// <summary>
		/// Reset counter to new value (use with caution).
		/// </summary>
		/// <param name="value">New counter value</param>
		void Reset(int32_t value);

	private:
		// Atomic counter value (acquire/release ordering)
		std::atomic<int32_t> m_Value;

		// Synchronization for Wait()
		std::mutex m_Mutex;
		std::condition_variable m_ConditionVariable;
	};

} // namespace Lunex
