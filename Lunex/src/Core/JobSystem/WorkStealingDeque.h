#pragma once

#include "Core/Core.h"
#include <deque>
#include <optional>
#include <mutex>

namespace Lunex {

	/// <summary>
	/// Per-worker job deque.
	///
	/// - Owner pushes/pops at the back (LIFO, cache-friendly)
	/// - Thieves steal from the front (FIFO, oldest/biggest work first)
	///
	/// Implementation note: this used to be a lock-free Chase-Lev deque, but it moved
	/// non-trivially-copyable items (std::function, Ref<>) out of a slot *before* winning
	/// the CAS in Steal(). A lost race left an empty job behind, which was silently
	/// skipped while its counter was decremented (lost work). A short mutex-protected
	/// critical section is correct, and at job granularity the contention is negligible.
	/// </summary>
	template<typename T>
	class WorkStealingDeque {
	public:
		explicit WorkStealingDeque(size_t /*initialCapacity*/ = 1024) {}
		~WorkStealingDeque() = default;

		WorkStealingDeque(const WorkStealingDeque&) = delete;
		WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
		WorkStealingDeque(WorkStealingDeque&&) = delete;
		WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;

		/// Owner only: push to the back
		void Push(T&& item) {
			std::scoped_lock lock(m_Mutex);
			m_Items.push_back(std::move(item));
		}

		/// Owner only: pop from the back (LIFO)
		std::optional<T> Pop() {
			std::scoped_lock lock(m_Mutex);
			if (m_Items.empty()) return std::nullopt;
			T item = std::move(m_Items.back());
			m_Items.pop_back();
			return item;
		}

		/// Any thread: steal from the front (FIFO)
		std::optional<T> Steal() {
			std::scoped_lock lock(m_Mutex);
			if (m_Items.empty()) return std::nullopt;
			T item = std::move(m_Items.front());
			m_Items.pop_front();
			return item;
		}

		size_t Size() const {
			std::scoped_lock lock(m_Mutex);
			return m_Items.size();
		}

		bool IsEmpty() const { return Size() == 0; }

	private:
		mutable std::mutex m_Mutex;
		std::deque<T> m_Items;
	};

} // namespace Lunex
