#include "stpch.h"
#include "JobCounter.h"

namespace Lunex {

	JobCounter::JobCounter(int32_t initialValue)
		: m_Value(initialValue) {
	}

	void JobCounter::Add(int32_t value) {
		// Release: ensure all prior writes are visible before counter update
		m_Value.fetch_add(value, std::memory_order_release);
	}

	void JobCounter::Decrement() {
		// Release: ensure job completion is visible before counter update
		int32_t oldValue = m_Value.fetch_sub(1, std::memory_order_release);

		// If this was the last job, notify all waiting threads
		if (oldValue == 1) {
			// Acquire: ensure we see all completed job writes
			std::scoped_lock lock(m_Mutex);
			m_ConditionVariable.notify_all();
		}
	}

	void JobCounter::Wait() {
		std::unique_lock lock(m_Mutex);

		// Wait until counter reaches zero
		// Acquire: ensure we see all completed job writes
		m_ConditionVariable.wait(lock, [this]() {
			return m_Value.load(std::memory_order_acquire) <= 0;
		});
	}

	void JobCounter::Reset(int32_t value) {
		// seq_cst: provide strongest guarantee for reset
		m_Value.store(value, std::memory_order_seq_cst);
	}

} // namespace Lunex
