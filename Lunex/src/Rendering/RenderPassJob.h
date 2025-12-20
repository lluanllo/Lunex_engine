#pragma once

/**
 * @file RenderPassJob.h
 * @brief Job-based render pass execution for parallel rendering
 * 
 * This file defines the interface for integrating render passes with
 * the JobSystem, enabling parallel command recording and pass execution.
 * 
 * Architecture:
 * - Each RenderPass can create a RenderPassJob for parallel execution
 * - Jobs record commands to thread-local command lists
 * - Main thread merges and submits command lists
 * 
 * Usage:
 * 1. Create RenderPassJob from a RenderPass
 * 2. Schedule job via JobSystem
 * 3. Wait for completion
 * 4. Execute recorded commands on main thread
 */

#include "Core/Core.h"
#include "Core/JobSystem/JobSystem.h"
#include "RenderPass.h"
#include "RHI/RHICommandList.h"

#include <functional>
#include <vector>
#include <atomic>

namespace Lunex {

	// Forward declarations
	class RenderPassBase;
	class RenderGraph;
	struct SceneRenderInfo;

	// ============================================================================
	// RENDER PASS JOB PRIORITY
	// ============================================================================
	
	/**
	 * @enum RenderPassPriority
	 * @brief Priority levels for render pass execution order
	 */
	enum class RenderPassPriority {
		Critical = 0,   // Must execute first (depth prepass, shadow maps)
		High = 1,       // Important passes (geometry, lighting)
		Normal = 2,     // Standard passes (post-processing)
		Low = 3         // Optional passes (debug visualization)
	};

	// ============================================================================
	// RENDER PASS JOB DATA
	// ============================================================================
	
	/**
	 * @struct RenderPassJobData
	 * @brief Data needed by a render pass job during execution
	 */
	struct RenderPassJobData {
		// Scene information
		const SceneRenderInfo* SceneInfo = nullptr;
		
		// Render graph resources
		const RenderPassResources* Resources = nullptr;
		
		// Command list for recording
		Ref<RHI::RHICommandList> CommandList;
		
		// Range for parallel iteration (e.g., entity indices)
		uint32_t StartIndex = 0;
		uint32_t EndIndex = 0;
		
		// Scene version for cancellation
		uint64_t SceneVersion = 0;
	};

	// ============================================================================
	// RENDER PASS JOB RESULT
	// ============================================================================
	
	/**
	 * @struct RenderPassJobResult
	 * @brief Result of a render pass job execution
	 */
	struct RenderPassJobResult {
		bool Success = false;
		uint32_t DrawCallsRecorded = 0;
		float ExecutionTimeMs = 0.0f;
		std::string ErrorMessage;
	};

	// ============================================================================
	// RENDER PASS JOB INTERFACE
	// ============================================================================
	
	/**
	 * @class IRenderPassJob
	 * @brief Interface for job-based render pass execution
	 * 
	 * Implementations allow render passes to execute work in parallel,
	 * recording commands to separate command lists that are later merged.
	 */
	class IRenderPassJob {
	public:
		virtual ~IRenderPassJob() = default;
		
		/**
		 * @brief Get the name of this job (for profiling)
		 */
		virtual const char* GetName() const = 0;
		
		/**
		 * @brief Get the priority of this job
		 */
		virtual RenderPassPriority GetPriority() const = 0;
		
		/**
		 * @brief Check if this job can execute in parallel with others
		 */
		virtual bool CanExecuteParallel() const = 0;
		
		/**
		 * @brief Get dependencies (jobs that must complete before this one)
		 */
		virtual const std::vector<IRenderPassJob*>& GetDependencies() const = 0;
		
		/**
		 * @brief Prepare the job for execution
		 * Called on main thread before scheduling
		 * @param data Job data including scene info and resources
		 */
		virtual void Prepare(const RenderPassJobData& data) = 0;
		
		/**
		 * @brief Execute the job (may run on worker thread)
		 * Records commands to the provided command list
		 */
		virtual void Execute() = 0;
		
		/**
		 * @brief Complete the job (called on main thread after Execute)
		 * Used for any main-thread-only operations
		 */
		virtual void Complete() = 0;
		
		/**
		 * @brief Get the result of execution
		 */
		virtual const RenderPassJobResult& GetResult() const = 0;
		
		/**
		 * @brief Get the recorded command list
		 */
		virtual Ref<RHI::RHICommandList> GetCommandList() const = 0;
		
		/**
		 * @brief Check if job has completed
		 */
		virtual bool IsComplete() const = 0;
	};

	// ============================================================================
	// RENDER PASS JOB BASE IMPLEMENTATION
	// ============================================================================
	
	/**
	 * @class RenderPassJobBase
	 * @brief Base implementation of IRenderPassJob
	 */
	class RenderPassJobBase : public IRenderPassJob {
	public:
		explicit RenderPassJobBase(const char* name, RenderPassPriority priority = RenderPassPriority::Normal);
		virtual ~RenderPassJobBase() = default;
		
		// IRenderPassJob implementation
		const char* GetName() const override { return m_Name; }
		RenderPassPriority GetPriority() const override { return m_Priority; }
		bool CanExecuteParallel() const override { return m_CanExecuteParallel; }
		const std::vector<IRenderPassJob*>& GetDependencies() const override { return m_Dependencies; }
		
		void Prepare(const RenderPassJobData& data) override;
		void Complete() override;
		
		const RenderPassJobResult& GetResult() const override { return m_Result; }
		Ref<RHI::RHICommandList> GetCommandList() const override { return m_CommandList; }
		bool IsComplete() const override { return m_Complete.load(std::memory_order_acquire); }
		
		// Configuration
		void SetCanExecuteParallel(bool canParallel) { m_CanExecuteParallel = canParallel; }
		void AddDependency(IRenderPassJob* dependency);
		
	protected:
		const char* m_Name = "UnnamedJob";
		RenderPassPriority m_Priority = RenderPassPriority::Normal;
		bool m_CanExecuteParallel = true;
		
		std::vector<IRenderPassJob*> m_Dependencies;
		RenderPassJobData m_Data;
		Ref<RHI::RHICommandList> m_CommandList;
		RenderPassJobResult m_Result;
		std::atomic<bool> m_Complete{false};
	};

	// ============================================================================
	// RENDER PASS JOB WRAPPER
	// ============================================================================
	
	/**
	 * @class RenderPassJob
	 * @brief Wraps a RenderPassBase for job-based execution
	 */
	class RenderPassJob : public RenderPassJobBase {
	public:
		RenderPassJob(RenderPassBase* pass, RenderGraph* graph);
		virtual ~RenderPassJob() = default;
		
		void Execute() override;
		
		/**
		 * @brief Get the wrapped render pass
		 */
		RenderPassBase* GetRenderPass() const { return m_Pass; }
		
	private:
		RenderPassBase* m_Pass = nullptr;
		RenderGraph* m_Graph = nullptr;
	};

	// ============================================================================
	// PARALLEL RENDER PASS JOB
	// ============================================================================
	
	/**
	 * @class ParallelRenderPassJob
	 * @brief Job that splits work across multiple worker threads
	 * 
	 * Used for passes that can parallelize over entities, draw calls, etc.
	 */
	class ParallelRenderPassJob : public RenderPassJobBase {
	public:
		using WorkFunc = std::function<void(uint32_t startIndex, uint32_t endIndex, 
		                                     RHI::RHICommandList* cmdList)>;
		
		ParallelRenderPassJob(const char* name, uint32_t workCount, WorkFunc workFunc);
		virtual ~ParallelRenderPassJob() = default;
		
		void Execute() override;
		
		/**
		 * @brief Set the grain size for parallel iteration
		 */
		void SetGrainSize(uint32_t grainSize) { m_GrainSize = grainSize; }
		
	private:
		WorkFunc m_WorkFunc;
		uint32_t m_WorkCount = 0;
		uint32_t m_GrainSize = 0;  // 0 = auto
		
		// Per-thread command lists
		std::vector<Ref<RHI::RHICommandList>> m_ThreadCommandLists;
	};

	// ============================================================================
	// RENDER JOB SCHEDULER
	// ============================================================================
	
	/**
	 * @class RenderJobScheduler
	 * @brief Schedules and manages render pass jobs
	 */
	class RenderJobScheduler {
	public:
		RenderJobScheduler();
		~RenderJobScheduler();
		
		/**
		 * @brief Add a job to the scheduler
		 */
		void AddJob(Ref<IRenderPassJob> job);
		
		/**
		 * @brief Clear all jobs
		 */
		void Clear();
		
		/**
		 * @brief Execute all jobs respecting dependencies
		 * @param sceneVersion Scene version for cancellation
		 * @return Counter to wait on for completion
		 */
		Ref<JobCounter> Execute(uint64_t sceneVersion = 0);
		
		/**
		 * @brief Wait for all jobs to complete
		 */
		void WaitForCompletion();
		
		/**
		 * @brief Get all recorded command lists (after completion)
		 */
		std::vector<Ref<RHI::RHICommandList>> GetCommandLists() const;
		
		/**
		 * @brief Get statistics
		 */
		struct Statistics {
			uint32_t TotalJobs = 0;
			uint32_t ParallelJobs = 0;
			uint32_t SequentialJobs = 0;
			float TotalExecutionTimeMs = 0.0f;
		};
		const Statistics& GetStatistics() const { return m_Stats; }
		
	private:
		void SortJobsByPriority();
		void BuildDependencyGraph();
		
		std::vector<Ref<IRenderPassJob>> m_Jobs;
		Ref<JobCounter> m_CompletionCounter;
		Statistics m_Stats;
	};

} // namespace Lunex
