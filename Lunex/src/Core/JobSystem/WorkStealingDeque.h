#pragma once

#include "Core/Core.h"
#include <atomic>
#include <vector>
#include <optional>
#include <mutex>

namespace Lunex {

	/// <summary>
	/// Lock-free work-stealing deque implementing Chase-Lev algorithm.
	/// 
	/// Owner thread operations (Push/Pop) are lock-free and use LIFO order.
	/// Thief threads (Steal) use lock-free CAS operations with FIFO order.
	/// 
	/// Memory Ordering:
	/// - Bottom (owner): relaxed for push, acquire for pop
	/// - Top (thieves): relaxed for steal attempt, seq_cst for successful CAS
	/// - Array: release/acquire for synchronization
	/// 
	/// Thread Safety:
	/// - ONE owner thread can Push/Pop without locks
	/// - MULTIPLE thief threads can Steal with CAS synchronization
	/// - Dynamic resizing protected by mutex (rare operation)
	/// 
	/// Usage Example:
	/// ```cpp
	/// WorkStealingDeque<Job> deque(1024);
	/// 
	/// // Owner thread (worker)
	/// deque.Push(Job{...});           // LIFO, lock-free
	/// auto job = deque.Pop();         // LIFO, lock-free
	/// 
	/// // Thief thread (work-stealing)
	/// auto stolen = deque.Steal();    // FIFO, lock-free CAS
	/// ```
	/// 
	/// References:
	/// - "Dynamic Circular Work-Stealing Deque" (Chase & Lev, 2005)
	/// - "Correct and Efficient Work-Stealing for Weak Memory Models" (Lê et al., 2013)
	/// </summary>
	template<typename T>
	class WorkStealingDeque {
	public:
		/// <summary>
		/// Creates a work-stealing deque with initial capacity.
		/// </summary>
		/// <param name="initialCapacity">Initial buffer size (must be power of 2)</param>
		explicit WorkStealingDeque(size_t initialCapacity = 1024);
		
		~WorkStealingDeque() = default;

		// Non-copyable, non-movable (contains atomics)
		WorkStealingDeque(const WorkStealingDeque&) = delete;
		WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
		WorkStealingDeque(WorkStealingDeque&&) = delete;
		WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;

		/// <summary>
		/// Push item to local end (owner thread only, LIFO).
		/// Lock-free operation with automatic resizing if needed.
		/// </summary>
		/// <param name="item">Item to push (moved)</param>
		void Push(T&& item);

		/// <summary>
		/// Pop item from local end (owner thread only, LIFO).
		/// Lock-free operation.
		/// </summary>
		/// <returns>Item if available, empty optional if deque is empty</returns>
		std::optional<T> Pop();

		/// <summary>
		/// Steal item from remote end (thief threads, FIFO).
		/// Lock-free CAS operation, may fail spuriously.
		/// </summary>
		/// <returns>Item if stolen successfully, empty optional otherwise</returns>
		std::optional<T> Steal();

		/// <summary>
		/// Get approximate size (may be stale due to concurrent operations).
		/// </summary>
		/// <returns>Approximate number of items in deque</returns>
		size_t Size() const {
			int64_t bottom = m_Bottom.load(std::memory_order_relaxed);
			int64_t top = m_Top.load(std::memory_order_relaxed);
			return static_cast<size_t>(std::max<int64_t>(0, bottom - top));
		}

		/// <summary>
		/// Check if deque is empty (approximate).
		/// </summary>
		bool IsEmpty() const {
			return Size() == 0;
		}

	private:
		/// <summary>
		/// Circular buffer wrapper with atomic pointer.
		/// Supports lock-free reads and atomic replacement.
		/// </summary>
		struct Array {
			std::vector<T> Data;
			size_t Mask; // Capacity - 1 (for fast modulo)

			explicit Array(size_t capacity)
				: Data(capacity), Mask(capacity - 1) {
				LNX_CORE_ASSERT((capacity & (capacity - 1)) == 0, "Capacity must be power of 2");
			}

			T& operator[](int64_t index) {
				return Data[index & Mask];
			}

			const T& operator[](int64_t index) const {
				return Data[index & Mask];
			}

			size_t Capacity() const {
				return Data.size();
			}
		};

		/// <summary>
		/// Grow buffer to double size (called by owner when full).
		/// Protected by mutex since it's a rare operation.
		/// </summary>
		void Grow();

	private:
		// Memory layout optimized for cache (owner variables together)
		alignas(64) std::atomic<int64_t> m_Bottom; // Owner's local end
		alignas(64) std::atomic<int64_t> m_Top;    // Thieves' remote end

		// Array pointer (atomic replacement for lock-free Grow)
		std::atomic<Array*> m_Array;

		// Resize mutex (only for Grow operation, rarely contended)
		std::mutex m_ResizeMutex;

		// Storage management
		std::vector<Scope<Array>> m_OldArrays; // Keep old arrays alive during growth
	};

	// ========================================================================
	// IMPLEMENTATION
	// ========================================================================

	template<typename T>
	WorkStealingDeque<T>::WorkStealingDeque(size_t initialCapacity)
		: m_Bottom(0), m_Top(0) {
		LNX_CORE_ASSERT((initialCapacity & (initialCapacity - 1)) == 0, "Initial capacity must be power of 2");
		
		auto initialArray = CreateScope<Array>(initialCapacity);
		m_Array.store(initialArray.get(), std::memory_order_relaxed);
		m_OldArrays.push_back(std::move(initialArray));
	}

	template<typename T>
	void WorkStealingDeque<T>::Push(T&& item) {
		// Load bottom (relaxed since we're the only writer)
		int64_t bottom = m_Bottom.load(std::memory_order_relaxed);
		int64_t top = m_Top.load(std::memory_order_acquire); // Synchronize with Steal

		Array* array = m_Array.load(std::memory_order_relaxed);

		// Check if full
		if (bottom - top >= static_cast<int64_t>(array->Capacity())) {
			Grow(); // Resize (mutex-protected)
			array = m_Array.load(std::memory_order_relaxed);
		}

		// Store item (move semantics)
		(*array)[bottom] = std::move(item);

		// Ensure item is written before incrementing bottom
		std::atomic_thread_fence(std::memory_order_release);
		
		// Increment bottom (release: publish item to stealers)
		m_Bottom.store(bottom + 1, std::memory_order_release);
	}

	template<typename T>
	std::optional<T> WorkStealingDeque<T>::Pop() {
		// Decrement bottom speculatively (we're the only writer)
		int64_t bottom = m_Bottom.load(std::memory_order_relaxed) - 1;
		Array* array = m_Array.load(std::memory_order_relaxed);

		// Update bottom (release: synchronize with Steal)
		m_Bottom.store(bottom, std::memory_order_release);

		// Synchronize with Steal operations
		std::atomic_thread_fence(std::memory_order_seq_cst);

		int64_t top = m_Top.load(std::memory_order_relaxed);

		std::optional<T> result;

		if (top < bottom) {
			// Deque is not empty, pop succeeded
			result = std::move((*array)[bottom]);
		}
		else if (top == bottom) {
			// Last item, race with stealers
			// Try to claim it with CAS (seq_cst for total order)
			if (m_Top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Won the race
				result = std::move((*array)[bottom]);
			}
			// Lost the race, deque is now empty
			m_Bottom.store(top + 1, std::memory_order_relaxed);
		}
		else {
			// Deque was already empty (top > bottom after decrement)
			m_Bottom.store(top, std::memory_order_relaxed);
		}

		return result;
	}

	template<typename T>
	std::optional<T> WorkStealingDeque<T>::Steal() {
		// Load top (acquire: synchronize with Push)
		int64_t top = m_Top.load(std::memory_order_acquire);

		// Ensure top is loaded before bottom
		std::atomic_thread_fence(std::memory_order_seq_cst);

		// Load bottom (acquire: synchronize with Pop)
		int64_t bottom = m_Bottom.load(std::memory_order_acquire);

		std::optional<T> result;

		if (top < bottom) {
			// Deque appears non-empty, try to steal
			Array* array = m_Array.load(std::memory_order_consume);

			// Read item (may be stale if race lost)
			T item = std::move((*array)[top]);

			// Try to claim item with CAS (seq_cst for total order with Pop)
			if (m_Top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Successfully stolen
				result = std::move(item);
			}
			// Failed CAS: another thief or owner got it
		}

		return result;
	}

	template<typename T>
	void WorkStealingDeque<T>::Grow() {
		// Mutex-protected resize (rare operation)
		std::scoped_lock lock(m_ResizeMutex);

		// Check if another thread already resized
		Array* oldArray = m_Array.load(std::memory_order_relaxed);
		size_t newCapacity = oldArray->Capacity() * 2;

		// Create new array
		auto newArray = CreateScope<Array>(newCapacity);

		// Copy elements from old to new
		int64_t bottom = m_Bottom.load(std::memory_order_relaxed);
		int64_t top = m_Top.load(std::memory_order_relaxed);

		for (int64_t i = top; i < bottom; ++i) {
			(*newArray)[i] = std::move((*oldArray)[i]);
		}

		// Atomically replace array (release: publish new array)
		m_Array.store(newArray.get(), std::memory_order_release);

		// Keep old array alive (in case of concurrent Steal operations)
		m_OldArrays.push_back(std::move(newArray));
	}

} // namespace Lunex
