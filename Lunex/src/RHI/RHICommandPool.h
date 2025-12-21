#pragma once

/**
 * @file RHICommandPool.h
 * @brief Command buffer allocation and pooling for multithreaded rendering
 * 
 * This provides:
 * - Per-thread command list allocation
 * - Command list pooling and reuse
 * - Deferred command submission
 * 
 * For AAA-level multithreaded rendering:
 * - Each worker thread gets its own pool
 * - Command lists are recorded in parallel
 * - All submitted together to GPU queue
 */

#include "RHICommandList.h"
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// COMMAND POOL CONFIGURATION
	// ============================================================================
	
	struct CommandPoolConfig {
		uint32_t InitialPoolSize = 4;    // Initial command lists per pool
		uint32_t MaxPoolSize = 64;       // Maximum command lists to keep
		bool ThreadSafe = true;          // Enable thread-safe allocation
	};

	// ============================================================================
	// RHI COMMAND POOL
	// ============================================================================
	
	/**
	 * @class RHICommandPool
	 * @brief Pool for allocating and recycling command lists
	 * 
	 * Usage:
	 * ```cpp
	 * auto pool = RHICommandPool::Create(config);
	 * 
	 * // In worker thread:
	 * auto cmdList = pool->Allocate();
	 * cmdList->Begin();
	 * // ... record commands ...
	 * cmdList->End();
	 * pool->Submit(cmdList);
	 * 
	 * // On main thread:
	 * pool->ExecuteAll(queue);
	 * pool->Reset(); // Recycles command lists for next frame
	 * ```
	 */
	class RHICommandPool {
	public:
		RHICommandPool(const CommandPoolConfig& config = CommandPoolConfig{});
		~RHICommandPool();
		
		/**
		 * @brief Allocate a command list from the pool
		 * Thread-safe if configured
		 */
		Ref<RHICommandList> Allocate();
		
		/**
		 * @brief Submit a completed command list for execution
		 * Thread-safe if configured
		 */
		void Submit(Ref<RHICommandList> cmdList);
		
		/**
		 * @brief Execute all submitted command lists on a queue
		 * Must be called from the main render thread
		 */
		void ExecuteAll(RHICommandQueue* queue);
		
		/**
		 * @brief Execute all submitted command lists immediately
		 * For OpenGL (no deferred execution)
		 */
		void ExecuteAllImmediate();
		
		/**
		 * @brief Reset the pool for next frame
		 * Recycles all used command lists back to available pool
		 */
		void Reset();
		
		/**
		 * @brief Get statistics
		 */
		struct Statistics {
			uint32_t TotalAllocated = 0;
			uint32_t CurrentlyUsed = 0;
			uint32_t PoolSize = 0;
			uint32_t SubmittedThisFrame = 0;
		};
		Statistics GetStatistics() const;
		
		// Factory
		static Ref<RHICommandPool> Create(const CommandPoolConfig& config = CommandPoolConfig{});
		
	private:
		CommandPoolConfig m_Config;
		
		std::vector<Ref<RHICommandList>> m_AvailablePool;
		std::vector<Ref<RHICommandList>> m_InUsePool;
		std::queue<Ref<RHICommandList>> m_PendingSubmission;
		
		mutable std::mutex m_PoolMutex;
		mutable std::mutex m_SubmitMutex;
		
		Statistics m_Stats;
	};

	// ============================================================================
	// PARALLEL COMMAND RECORDER
	// ============================================================================
	
	/**
	 * @class ParallelCommandRecorder
	 * @brief Helper for recording commands in parallel across threads
	 * 
	 * Usage:
	 * ```cpp
	 * ParallelCommandRecorder recorder(numThreads);
	 * 
	 * // Record in parallel
	 * recorder.RecordParallel([](RHICommandList& cmd, uint32_t threadIdx) {
	 *     // Each thread records its portion
	 *     cmd.Begin();
	 *     // ... record thread's work ...
	 *     cmd.End();
	 * });
	 * 
	 * // Execute all on main thread
	 * recorder.ExecuteAll(queue);
	 * ```
	 */
	class ParallelCommandRecorder {
	public:
		using RecordFunction = std::function<void(RHICommandList&, uint32_t threadIdx)>;
		
		ParallelCommandRecorder(uint32_t numThreads = 0);  // 0 = auto-detect
		~ParallelCommandRecorder();
		
		/**
		 * @brief Get number of threads
		 */
		uint32_t GetNumThreads() const { return m_NumThreads; }
		
		/**
		 * @brief Allocate command lists for all threads
		 */
		void Prepare();
		
		/**
		 * @brief Get command list for a specific thread
		 */
		RHICommandList* GetCommandList(uint32_t threadIdx);
		
		/**
		 * @brief Record commands in parallel
		 * This blocks until all threads complete
		 */
		void RecordParallel(const RecordFunction& recordFunc);
		
		/**
		 * @brief Submit all command lists for execution
		 */
		void ExecuteAll(RHICommandQueue* queue);
		
		/**
		 * @brief Reset for next frame
		 */
		void Reset();
		
	private:
		uint32_t m_NumThreads;
		Ref<RHICommandPool> m_Pool;
		std::vector<Ref<RHICommandList>> m_ThreadCommandLists;
	};

	// ============================================================================
	// DEFERRED COMMAND BUFFER
	// ============================================================================
	
	/**
	 * @class DeferredCommandBuffer
	 * @brief Records high-level commands for deferred execution
	 * 
	 * Unlike RHICommandList which records GPU commands,
	 * this records higher-level operations that can be:
	 * - Serialized
	 * - Replayed
	 * - Modified before execution
	 * 
	 * Useful for:
	 * - Recording rendering commands from game thread
	 * - Command reordering and optimization
	 * - Debug recording/playback
	 */
	class DeferredCommandBuffer {
	public:
		// Command types
		enum class CommandType : uint8_t {
			SetPipeline,
			SetVertexBuffer,
			SetIndexBuffer,
			SetTexture,
			SetUniformBuffer,
			DrawIndexed,
			Draw,
			Dispatch,
			BeginRenderPass,
			EndRenderPass,
			ResourceBarrier,
			Custom
		};
		
		// Generic command structure
		struct Command {
			CommandType Type;
			uint64_t Data[8];  // Generic data storage
		};
		
		/**
		 * @brief Record a draw indexed command
		 */
		void DrawIndexed(const DrawArgs& args);
		
		/**
		 * @brief Record a draw command
		 */
		void Draw(const DrawArrayArgs& args);
		
		/**
		 * @brief Record set pipeline command
		 */
		void SetPipeline(RHIGraphicsPipeline* pipeline);
		
		/**
		 * @brief Execute all recorded commands
		 */
		void Execute(RHICommandList* cmdList);
		
		/**
		 * @brief Clear all commands
		 */
		void Clear();
		
		/**
		 * @brief Get command count
		 */
		size_t GetCommandCount() const { return m_Commands.size(); }
		
	private:
		std::vector<Command> m_Commands;
	};

} // namespace RHI
} // namespace Lunex
