#include "stpch.h"
#include "AssetLoader.h"
#include "Log/Log.h"

namespace Lunex {

	AssetLoader& AssetLoader::Get() {
		static AssetLoader instance;
		return instance;
	}

	void AssetLoader::Initialize() {
		if (m_Initialized) return;
		
		LNX_LOG_INFO("AssetLoader initialized with async support");
		m_Initialized = true;
	}

	void AssetLoader::Shutdown() {
		WaitForAll();
		
		m_PendingJobs.clear();
		m_CompletedJobs.clear();
		m_Initialized = false;
		
		LNX_LOG_INFO("AssetLoader shutdown");
	}

	void AssetLoader::CancelLoad(UUID assetID) {
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		
		m_PendingJobs.erase(
			std::remove_if(m_PendingJobs.begin(), m_PendingJobs.end(),
				[assetID](const Ref<AssetLoadJob>& job) {
					return job->AssetID == assetID && !job->IsComplete;
				}),
			m_PendingJobs.end()
		);
	}

	void AssetLoader::WaitForAll() {
		while (true) {
			{
				std::lock_guard<std::mutex> lock(m_JobsMutex);
				
				bool allComplete = true;
				for (const auto& job : m_PendingJobs) {
					if (!job->IsComplete) {
						allComplete = false;
						break;
					}
				}
				
				if (allComplete || m_PendingJobs.empty()) break;
			}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	size_t AssetLoader::GetPendingCount() const {
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		
		size_t count = 0;
		for (const auto& job : m_PendingJobs) {
			if (!job->IsComplete) count++;
		}
		return count;
	}

	size_t AssetLoader::GetCompletedCount() const {
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		return m_CompletedJobs.size();
	}

	float AssetLoader::GetProgress() const {
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		
		if (m_PendingJobs.empty() && m_CompletedJobs.empty()) {
			return 1.0f;
		}
		
		size_t total = m_PendingJobs.size() + m_CompletedJobs.size();
		return total > 0 ? (float)m_CompletedJobs.size() / (float)total : 1.0f;
	}

	void AssetLoader::Update() {
		std::vector<Ref<AssetLoadJob>> completed;
		
		{
			std::lock_guard<std::mutex> lock(m_JobsMutex);
			completed = std::move(m_CompletedJobs);
			m_CompletedJobs.clear();
		}
		
		// Invoke callbacks on main thread
		for (auto& job : completed) {
			if (job->Callback) {
				job->Callback(job->LoadedAsset);
			}
		}
		
		// Clean up pending jobs that are complete
		{
			std::lock_guard<std::mutex> lock(m_JobsMutex);
			m_PendingJobs.erase(
				std::remove_if(m_PendingJobs.begin(), m_PendingJobs.end(),
					[](const Ref<AssetLoadJob>& job) { return job->IsComplete.load(); }),
				m_PendingJobs.end()
			);
		}
	}

	Ref<Asset> AssetLoader::LoadAssetInternal(const std::filesystem::path& path, AssetType type) {
		// This is a simplified implementation
		// In a full implementation, you would dispatch to specific loaders
		
		LNX_LOG_INFO("Loading asset async: {0}", path.string());
		
		// For now, return nullptr - derived asset types should implement LoadFromFile
		return nullptr;
	}

} // namespace Lunex
