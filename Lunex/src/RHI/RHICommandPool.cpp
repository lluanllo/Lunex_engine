#include "stpch.h"
#include "RHICommandPool.h"
#include "Log/Log.h"
#include <algorithm>
#include <future>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// RHI COMMAND POOL IMPLEMENTATION
	// ============================================================================
	
	RHICommandPool::RHICommandPool(const CommandPoolConfig& config)
		: m_Config(config)
	{
		// Pre-allocate initial pool
		m_AvailablePool.reserve(config.InitialPoolSize);
		for (uint32_t i = 0; i < config.InitialPoolSize; i++) {
			m_AvailablePool.push_back(RHICommandList::CreateGraphics());
			m_Stats.TotalAllocated++;
		}
		m_Stats.PoolSize = config.InitialPoolSize;
		
		LNX_LOG_INFO("Created RHI Command Pool with {0} initial command lists", config.InitialPoolSize);
	}
	
	RHICommandPool::~RHICommandPool() {
		// Wait for any pending submissions
		Reset();
	}
	
	Ref<RHICommandList> RHICommandPool::Allocate() {
		Ref<RHICommandList> cmdList = nullptr;
		
		if (m_Config.ThreadSafe) {
			std::lock_guard<std::mutex> lock(m_PoolMutex);
			
			if (!m_AvailablePool.empty()) {
				cmdList = m_AvailablePool.back();
				m_AvailablePool.pop_back();
			}
		} else {
			if (!m_AvailablePool.empty()) {
				cmdList = m_AvailablePool.back();
				m_AvailablePool.pop_back();
			}
		}
		
		// If pool empty, create new
		if (!cmdList) {
			cmdList = RHICommandList::CreateGraphics();
			
			if (m_Config.ThreadSafe) {
				std::lock_guard<std::mutex> lock(m_PoolMutex);
				m_Stats.TotalAllocated++;
			} else {
				m_Stats.TotalAllocated++;
			}
		}
		
		// Reset for fresh use
		cmdList->Reset();
		
		// Track as in-use
		if (m_Config.ThreadSafe) {
			std::lock_guard<std::mutex> lock(m_PoolMutex);
			m_InUsePool.push_back(cmdList);
			m_Stats.CurrentlyUsed = static_cast<uint32_t>(m_InUsePool.size());
		} else {
			m_InUsePool.push_back(cmdList);
			m_Stats.CurrentlyUsed = static_cast<uint32_t>(m_InUsePool.size());
		}
		
		return cmdList;
	}
	
	void RHICommandPool::Submit(Ref<RHICommandList> cmdList) {
		if (!cmdList) return;
		
		if (m_Config.ThreadSafe) {
			std::lock_guard<std::mutex> lock(m_SubmitMutex);
			m_PendingSubmission.push(cmdList);
			m_Stats.SubmittedThisFrame++;
		} else {
			m_PendingSubmission.push(cmdList);
			m_Stats.SubmittedThisFrame++;
		}
	}
	
	void RHICommandPool::ExecuteAll(RHICommandQueue* queue) {
		if (!queue) {
			LNX_LOG_ERROR("CommandPool::ExecuteAll called with null queue");
			return;
		}
		
		std::vector<RHICommandList*> cmdLists;
		
		{
			std::lock_guard<std::mutex> lock(m_SubmitMutex);
			cmdLists.reserve(m_PendingSubmission.size());
			
			while (!m_PendingSubmission.empty()) {
				cmdLists.push_back(m_PendingSubmission.front().get());
				m_PendingSubmission.pop();
			}
		}
		
		if (!cmdLists.empty()) {
			queue->Submit(cmdLists.data(), static_cast<uint32_t>(cmdLists.size()));
		}
	}
	
	void RHICommandPool::ExecuteAllImmediate() {
		// For OpenGL - execute each command list immediately
		// OpenGL doesn't have true deferred command lists
		
		std::vector<Ref<RHICommandList>> toExecute;
		
		{
			std::lock_guard<std::mutex> lock(m_SubmitMutex);
			while (!m_PendingSubmission.empty()) {
				toExecute.push_back(m_PendingSubmission.front());
				m_PendingSubmission.pop();
			}
		}
		
		// In OpenGL, the commands were already executed during recording
		// This is just for consistency with the API
	}
	
	void RHICommandPool::Reset() {
		// Wait for GPU to finish (in case of async)
		// For OpenGL this is not needed, but for Vulkan/DX12 it would be
		
		{
			std::lock_guard<std::mutex> lock(m_PoolMutex);
			
			// Move all in-use back to available (up to max pool size)
			for (auto& cmdList : m_InUsePool) {
				cmdList->Reset();
				
				if (m_AvailablePool.size() < m_Config.MaxPoolSize) {
					m_AvailablePool.push_back(cmdList);
				}
				// Else: let it be destroyed (trim pool)
			}
			m_InUsePool.clear();
			
			m_Stats.CurrentlyUsed = 0;
			m_Stats.PoolSize = static_cast<uint32_t>(m_AvailablePool.size());
		}
		
		{
			std::lock_guard<std::mutex> lock(m_SubmitMutex);
			while (!m_PendingSubmission.empty()) {
				m_PendingSubmission.pop();
			}
			m_Stats.SubmittedThisFrame = 0;
		}
	}
	
	RHICommandPool::Statistics RHICommandPool::GetStatistics() const {
		if (m_Config.ThreadSafe) {
			std::lock_guard<std::mutex> lock(m_PoolMutex);
			return m_Stats;
		}
		return m_Stats;
	}
	
	Ref<RHICommandPool> RHICommandPool::Create(const CommandPoolConfig& config) {
		return CreateRef<RHICommandPool>(config);
	}

	// ============================================================================
	// PARALLEL COMMAND RECORDER IMPLEMENTATION
	// ============================================================================
	
	ParallelCommandRecorder::ParallelCommandRecorder(uint32_t numThreads) {
		if (numThreads == 0) {
			numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
		}
		m_NumThreads = numThreads;
		
		CommandPoolConfig config;
		config.InitialPoolSize = numThreads;
		config.ThreadSafe = true;
		m_Pool = RHICommandPool::Create(config);
		
		m_ThreadCommandLists.resize(numThreads);
		
		LNX_LOG_INFO("Created ParallelCommandRecorder with {0} threads", numThreads);
	}
	
	ParallelCommandRecorder::~ParallelCommandRecorder() {
		Reset();
	}
	
	void ParallelCommandRecorder::Prepare() {
		for (uint32_t i = 0; i < m_NumThreads; i++) {
			m_ThreadCommandLists[i] = m_Pool->Allocate();
		}
	}
	
	RHICommandList* ParallelCommandRecorder::GetCommandList(uint32_t threadIdx) {
		if (threadIdx >= m_NumThreads) {
			return nullptr;
		}
		
		if (!m_ThreadCommandLists[threadIdx]) {
			m_ThreadCommandLists[threadIdx] = m_Pool->Allocate();
		}
		
		return m_ThreadCommandLists[threadIdx].get();
	}
	
	void ParallelCommandRecorder::RecordParallel(const RecordFunction& recordFunc) {
		Prepare();
		
		// Launch parallel recording tasks
		std::vector<std::future<void>> futures;
		futures.reserve(m_NumThreads);
		
		for (uint32_t i = 0; i < m_NumThreads; i++) {
			futures.push_back(std::async(std::launch::async, [this, i, &recordFunc]() {
				RHICommandList* cmdList = m_ThreadCommandLists[i].get();
				if (cmdList) {
					recordFunc(*cmdList, i);
				}
			}));
		}
		
		// Wait for all to complete
		for (auto& future : futures) {
			future.get();
		}
		
		// Submit all for execution
		for (auto& cmdList : m_ThreadCommandLists) {
			if (cmdList) {
				m_Pool->Submit(cmdList);
			}
		}
	}
	
	void ParallelCommandRecorder::ExecuteAll(RHICommandQueue* queue) {
		m_Pool->ExecuteAll(queue);
	}
	
	void ParallelCommandRecorder::Reset() {
		m_ThreadCommandLists.clear();
		m_ThreadCommandLists.resize(m_NumThreads);
		m_Pool->Reset();
	}

	// ============================================================================
	// DEFERRED COMMAND BUFFER IMPLEMENTATION
	// ============================================================================
	
	void DeferredCommandBuffer::DrawIndexed(const DrawArgs& args) {
		Command cmd;
		cmd.Type = CommandType::DrawIndexed;
		cmd.Data[0] = args.IndexCount;
		cmd.Data[1] = args.InstanceCount;
		cmd.Data[2] = args.FirstIndex;
		cmd.Data[3] = static_cast<uint64_t>(args.VertexOffset);
		cmd.Data[4] = args.FirstInstance;
		m_Commands.push_back(cmd);
	}
	
	void DeferredCommandBuffer::Draw(const DrawArrayArgs& args) {
		Command cmd;
		cmd.Type = CommandType::Draw;
		cmd.Data[0] = args.VertexCount;
		cmd.Data[1] = args.InstanceCount;
		cmd.Data[2] = args.FirstVertex;
		cmd.Data[3] = args.FirstInstance;
		m_Commands.push_back(cmd);
	}
	
	void DeferredCommandBuffer::SetPipeline(RHIGraphicsPipeline* pipeline) {
		Command cmd;
		cmd.Type = CommandType::SetPipeline;
		cmd.Data[0] = reinterpret_cast<uint64_t>(pipeline);
		m_Commands.push_back(cmd);
	}
	
	void DeferredCommandBuffer::Execute(RHICommandList* cmdList) {
		if (!cmdList) return;
		
		for (const auto& cmd : m_Commands) {
			switch (cmd.Type) {
				case CommandType::DrawIndexed: {
					DrawArgs args;
					args.IndexCount = static_cast<uint32_t>(cmd.Data[0]);
					args.InstanceCount = static_cast<uint32_t>(cmd.Data[1]);
					args.FirstIndex = static_cast<uint32_t>(cmd.Data[2]);
					args.VertexOffset = static_cast<int32_t>(cmd.Data[3]);
					args.FirstInstance = static_cast<uint32_t>(cmd.Data[4]);
					cmdList->DrawIndexed(args);
					break;
				}
				
				case CommandType::Draw: {
					DrawArrayArgs args;
					args.VertexCount = static_cast<uint32_t>(cmd.Data[0]);
					args.InstanceCount = static_cast<uint32_t>(cmd.Data[1]);
					args.FirstVertex = static_cast<uint32_t>(cmd.Data[2]);
					args.FirstInstance = static_cast<uint32_t>(cmd.Data[3]);
					cmdList->Draw(args);
					break;
				}
				
				case CommandType::SetPipeline: {
					auto* pipeline = reinterpret_cast<RHIGraphicsPipeline*>(cmd.Data[0]);
					cmdList->SetPipeline(pipeline);
					break;
				}
				
				// Add more command types as needed
				
				default:
					break;
			}
		}
	}
	
	void DeferredCommandBuffer::Clear() {
		m_Commands.clear();
	}

} // namespace RHI
} // namespace Lunex
