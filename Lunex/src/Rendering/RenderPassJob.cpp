#include "stpch.h"
#include "RenderPassJob.h"
#include "RenderGraph.h"
#include "Log/Log.h"

#include <chrono>
#include <algorithm>

namespace Lunex {

	// ============================================================================
	// RENDER PASS JOB BASE IMPLEMENTATION
	// ============================================================================
	
	RenderPassJobBase::RenderPassJobBase(const char* name, RenderPassPriority priority)
		: m_Name(name), m_Priority(priority) {
	}
	
	void RenderPassJobBase::Prepare(const RenderPassJobData& data) {
		m_Data = data;
		m_Complete.store(false, std::memory_order_release);
		m_Result = RenderPassJobResult{};
		
		// Create command list if not provided
		if (!m_Data.CommandList) {
			m_CommandList = RHI::RHICommandList::CreateGraphics();
		} else {
			m_CommandList = m_Data.CommandList;
		}
	}
	
	void RenderPassJobBase::Complete() {
		m_Complete.store(true, std::memory_order_release);
	}
	
	void RenderPassJobBase::AddDependency(IRenderPassJob* dependency) {
		if (dependency && std::find(m_Dependencies.begin(), m_Dependencies.end(), dependency) == m_Dependencies.end()) {
			m_Dependencies.push_back(dependency);
		}
	}

	// ============================================================================
	// RENDER PASS JOB WRAPPER IMPLEMENTATION
	// ============================================================================
	
	RenderPassJob::RenderPassJob(RenderPassBase* pass, RenderGraph* graph)
		: RenderPassJobBase(pass ? pass->GetName() : "NullPass", RenderPassPriority::Normal)
		, m_Pass(pass)
		, m_Graph(graph) {
		
		// Most render passes must execute sequentially due to OpenGL state
		// This can be overridden for passes that only record commands
		m_CanExecuteParallel = false;
	}
	
	void RenderPassJob::Execute() {
		if (!m_Pass || !m_Data.SceneInfo) {
			m_Result.Success = false;
			m_Result.ErrorMessage = "Invalid pass or scene info";
			return;
		}
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		try {
			// Begin command list recording
			if (m_CommandList) {
				m_CommandList->Begin();
			}
			
			// Execute the render pass
			RenderPassResources resources;
			if (m_Data.Resources) {
				resources = *m_Data.Resources;
			}
			
			m_Pass->Execute(resources, *m_Data.SceneInfo);
			
			// End command list recording
			if (m_CommandList) {
				m_CommandList->End();
			}
			
			m_Result.Success = true;
		}
		catch (const std::exception& e) {
			m_Result.Success = false;
			m_Result.ErrorMessage = e.what();
			LNX_LOG_ERROR("RenderPassJob::Execute failed for '{}': {}", m_Name, e.what());
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		m_Result.ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
	}

	// ============================================================================
	// PARALLEL RENDER PASS JOB IMPLEMENTATION
	// ============================================================================
	
	ParallelRenderPassJob::ParallelRenderPassJob(const char* name, uint32_t workCount, WorkFunc workFunc)
		: RenderPassJobBase(name, RenderPassPriority::Normal)
		, m_WorkFunc(std::move(workFunc))
		, m_WorkCount(workCount) {
		
		m_CanExecuteParallel = true;
	}
	
	void ParallelRenderPassJob::Execute() {
		if (!m_WorkFunc || m_WorkCount == 0) {
			m_Result.Success = true;
			return;
		}
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		try {
			// Use JobSystem's ParallelFor for work distribution
			auto& jobSystem = JobSystem::Get();
			
			// Calculate grain size if not set
			uint32_t grainSize = m_GrainSize;
			if (grainSize == 0) {
				// Auto-calculate: aim for ~4 jobs per worker
				uint32_t numWorkers = std::max(1u, std::thread::hardware_concurrency() - 1);
				grainSize = std::max(1u, m_WorkCount / (numWorkers * 4));
			}
			
			// Create per-chunk command lists
			uint32_t numChunks = (m_WorkCount + grainSize - 1) / grainSize;
			m_ThreadCommandLists.resize(numChunks);
			
			for (uint32_t i = 0; i < numChunks; i++) {
				m_ThreadCommandLists[i] = RHI::RHICommandList::CreateGraphics();
			}
			
			// Execute work in parallel
			auto counter = jobSystem.ParallelFor(
				0, m_WorkCount,
				[this, grainSize](uint32_t index) {
					uint32_t chunkIndex = index / grainSize;
					uint32_t localStart = index;
					uint32_t localEnd = std::min(index + 1, m_WorkCount);
					
					if (chunkIndex < m_ThreadCommandLists.size()) {
						m_WorkFunc(localStart, localEnd, m_ThreadCommandLists[chunkIndex].get());
					}
				},
				grainSize,
				JobPriority::High,
				m_Data.SceneVersion
			);
			
			// Wait for completion
			jobSystem.Wait(counter);
			
			m_Result.Success = true;
		}
		catch (const std::exception& e) {
			m_Result.Success = false;
			m_Result.ErrorMessage = e.what();
			LNX_LOG_ERROR("ParallelRenderPassJob::Execute failed for '{}': {}", m_Name, e.what());
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		m_Result.ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
	}

	// ============================================================================
	// RENDER JOB SCHEDULER IMPLEMENTATION
	// ============================================================================
	
	RenderJobScheduler::RenderJobScheduler() {
	}
	
	RenderJobScheduler::~RenderJobScheduler() {
		Clear();
	}
	
	void RenderJobScheduler::AddJob(Ref<IRenderPassJob> job) {
		if (job) {
			m_Jobs.push_back(std::move(job));
		}
	}
	
	void RenderJobScheduler::Clear() {
		m_Jobs.clear();
		m_CompletionCounter.reset();
		m_Stats = Statistics{};
	}
	
	Ref<JobCounter> RenderJobScheduler::Execute(uint64_t sceneVersion) {
		if (m_Jobs.empty()) {
			return JobSystem::Get().CreateCounter(0);
		}
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// Sort jobs by priority
		SortJobsByPriority();
		
		// Update statistics
		m_Stats.TotalJobs = static_cast<uint32_t>(m_Jobs.size());
		m_Stats.ParallelJobs = 0;
		m_Stats.SequentialJobs = 0;
		
		for (const auto& job : m_Jobs) {
			if (job->CanExecuteParallel()) {
				m_Stats.ParallelJobs++;
			} else {
				m_Stats.SequentialJobs++;
			}
		}
		
		// Create completion counter
		auto& jobSystem = JobSystem::Get();
		m_CompletionCounter = jobSystem.CreateCounter(static_cast<int32_t>(m_Jobs.size()));
		
		// Schedule jobs
		// For now, execute sequentially respecting dependencies
		// TODO: Implement proper dependency graph scheduling
		
		for (auto& job : m_Jobs) {
			// Check dependencies are complete
			const auto& deps = job->GetDependencies();
			for (auto* dep : deps) {
				while (!dep->IsComplete()) {
					std::this_thread::yield();
				}
			}
			
			// Prepare job data
			RenderPassJobData data;
			data.SceneVersion = sceneVersion;
			job->Prepare(data);
			
			if (job->CanExecuteParallel()) {
				// Schedule on worker thread
				jobSystem.Schedule(
					[&job, counter = m_CompletionCounter]() {
						job->Execute();
						job->Complete();
					},
					nullptr,
					m_CompletionCounter,
					JobPriority::High,
					sceneVersion
				);
			} else {
				// Execute immediately on current thread (for OpenGL state safety)
				job->Execute();
				job->Complete();
				m_CompletionCounter->Decrement();
			}
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		m_Stats.TotalExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		
		return m_CompletionCounter;
	}
	
	void RenderJobScheduler::WaitForCompletion() {
		if (m_CompletionCounter) {
			JobSystem::Get().Wait(m_CompletionCounter);
		}
	}
	
	std::vector<Ref<RHI::RHICommandList>> RenderJobScheduler::GetCommandLists() const {
		std::vector<Ref<RHI::RHICommandList>> result;
		result.reserve(m_Jobs.size());
		
		for (const auto& job : m_Jobs) {
			auto cmdList = job->GetCommandList();
			if (cmdList) {
				result.push_back(cmdList);
			}
		}
		
		return result;
	}
	
	void RenderJobScheduler::SortJobsByPriority() {
		std::stable_sort(m_Jobs.begin(), m_Jobs.end(),
			[](const Ref<IRenderPassJob>& a, const Ref<IRenderPassJob>& b) {
				return static_cast<int>(a->GetPriority()) < static_cast<int>(b->GetPriority());
			}
		);
	}
	
	void RenderJobScheduler::BuildDependencyGraph() {
		// TODO: Build a proper DAG for dependency resolution
		// For now, dependencies are checked linearly during execution
	}

} // namespace Lunex
