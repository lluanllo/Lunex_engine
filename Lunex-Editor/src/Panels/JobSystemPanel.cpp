#include "JobSystemPanel.h"
#include <chrono>
#include <thread>

namespace Lunex {

	JobSystemPanel::JobSystemPanel() {
		m_ThroughputHistory.reserve(m_HistorySize);
		m_LatencyHistory.reserve(m_HistorySize);
	}

	void JobSystemPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		ImGui::Begin("Job System Monitor", &m_IsOpen, ImGuiWindowFlags_None);

		// Get current metrics
		JobMetricsSnapshot metrics = JobSystem::Get().GetMetrics();

		// Draw sections
		DrawOverview(metrics);
		ImGui::Separator();
		DrawWorkerStatus(metrics);
		ImGui::Separator();
		DrawQueueStatus(metrics);
		ImGui::Separator();
		DrawPerformanceGraphs();
		ImGui::Separator();
		DrawTestControls();

		ImGui::End();
	}

	void JobSystemPanel::DrawOverview(const JobMetricsSnapshot& metrics) {
		if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();

			// Total jobs
			uint64_t scheduled = metrics.TotalJobsScheduled;
			uint64_t completed = metrics.TotalJobsCompleted;
			uint64_t stolen = metrics.TotalJobsStolen;

			ImGui::Text("Total Jobs Scheduled: %llu", scheduled);
			ImGui::Text("Total Jobs Completed: %llu", completed);
			ImGui::Text("Total Jobs Stolen:    %llu", stolen);

			// Progress bar
			float progress = scheduled > 0 ? (float)completed / (float)scheduled : 1.0f;
			ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

			ImGui::Spacing();

			// Throughput
			float throughput = metrics.Throughput;
			ImGui::Text("Throughput: %.1f jobs/sec", throughput);

			// Update history
			if (m_ThroughputHistory.size() >= m_HistorySize) {
				m_ThroughputHistory.erase(m_ThroughputHistory.begin());
			}
			m_ThroughputHistory.push_back(throughput);

			// Latency
			float latency = metrics.AvgJobLatencyMs;
			ImGui::Text("Avg Latency: %.2f ms", latency);

			// Update history
			if (m_LatencyHistory.size() >= m_HistorySize) {
				m_LatencyHistory.erase(m_LatencyHistory.begin());
			}
			m_LatencyHistory.push_back(latency);

			// Work stealing efficiency
			if (completed > 0) {
				float stealRate = (float)stolen / (float)completed * 100.0f;
				ImGui::Text("Work Steal Rate: %.1f%%", stealRate);
			}

			ImGui::Unindent();
		}
	}

	void JobSystemPanel::DrawWorkerStatus(const JobMetricsSnapshot& metrics) {
		if (ImGui::CollapsingHeader("Worker Threads", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();

			uint32_t active = metrics.ActiveWorkers;
			uint32_t idle = metrics.IdleWorkers;
			uint32_t total = active + idle;

			ImGui::Text("Active Workers: %u / %u", active, total);
			ImGui::Text("Idle Workers:   %u / %u", idle, total);

			// Worker utilization bar
			if (total > 0) {
				float utilization = (float)active / (float)total;
				ImGui::ProgressBar(utilization, ImVec2(-1, 0), "");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::Text("%.0f%% Utilization", utilization * 100.0f);
			}

			ImGui::Spacing();

			// Per-worker queue sizes
			ImGui::Text("Per-Worker Queue Sizes:");
			for (size_t i = 0; i < 16; ++i) {
				uint32_t size = metrics.WorkerQueueSizes[i];
				if (size > 0 || i < 8) { // Show first 8 or non-zero
					ImGui::Text("  Worker %zu: %u jobs", i, size);
				}
			}

			ImGui::Unindent();
		}
	}

	void JobSystemPanel::DrawQueueStatus(const JobMetricsSnapshot& metrics) {
		if (ImGui::CollapsingHeader("Queues", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();

			uint32_t globalSize = metrics.GlobalQueueSize;
			uint32_t commandSize = metrics.CommandBufferSize;

			ImGui::Text("Global Queue:         %u jobs", globalSize);
			ImGui::Text("Main-Thread Commands: %u pending", commandSize);

			// Visual queue indicators
			ImGui::Spacing();

			// Global queue bar
			ImGui::Text("Global Queue:");
			float globalProgress = std::min(1.0f, (float)globalSize / 100.0f);
			ImGui::ProgressBar(globalProgress, ImVec2(-1, 0), "");

			// Command buffer bar
			ImGui::Text("Command Buffer:");
			float commandProgress = std::min(1.0f, (float)commandSize / 50.0f);
			ImGui::ProgressBar(commandProgress, ImVec2(-1, 0), "");

			ImGui::Unindent();
		}
	}

	void JobSystemPanel::DrawPerformanceGraphs() {
		if (ImGui::CollapsingHeader("Performance History", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();

			// Throughput graph
			ImGui::Text("Throughput (jobs/sec):");
			if (!m_ThroughputHistory.empty()) {
				ImGui::PlotLines(
					"##Throughput",
					m_ThroughputHistory.data(),
					static_cast<int>(m_ThroughputHistory.size()),
					0,
					nullptr,
					0.0f,
					FLT_MAX,
					ImVec2(0, 80)
				);
			}

			ImGui::Spacing();

			// Latency graph
			ImGui::Text("Latency (ms):");
			if (!m_LatencyHistory.empty()) {
				ImGui::PlotLines(
					"##Latency",
					m_LatencyHistory.data(),
					static_cast<int>(m_LatencyHistory.size()),
					0,
					nullptr,
					0.0f,
					FLT_MAX,
					ImVec2(0, 80)
				);
			}

			ImGui::Unindent();
		}
	}

	void JobSystemPanel::DrawTestControls() {
		if (ImGui::CollapsingHeader("Test Controls")) {
			ImGui::Indent();

			ImGui::Text("Stress Test:");
			ImGui::SliderInt("Job Count", &m_TestJobCount, 10, 10000);
			ImGui::SliderInt("Job Duration (ms)", &m_TestJobDuration, 0, 100);

			if (ImGui::Button("Run Test Jobs", ImVec2(-1, 0))) {
				LNX_LOG_INFO("Running {0} test jobs...", m_TestJobCount);

				auto counter = JobSystem::Get().CreateCounter(m_TestJobCount);

				for (int i = 0; i < m_TestJobCount; ++i) {
					JobSystem::Get().Schedule(
						[duration = m_TestJobDuration, counter]() {
							// Simulate work
							if (duration > 0) {
								std::this_thread::sleep_for(std::chrono::milliseconds(duration));
							}

							// Dummy computation
							volatile int result = 0;
							for (int j = 0; j < 1000; ++j) {
								result += j * j;
							}

							counter->Decrement();
						},
						nullptr,
						nullptr,
						JobPriority::Normal
					);
				}

				LNX_LOG_INFO("Test jobs scheduled. Check metrics for results.");
			}

			ImGui::Spacing();

			if (ImGui::Button("Reset Metrics", ImVec2(-1, 0))) {
				JobSystem::Get().ResetMetrics();
				m_ThroughputHistory.clear();
				m_LatencyHistory.clear();
				LNX_LOG_INFO("Metrics reset");
			}

			ImGui::Unindent();
		}
	}

} // namespace Lunex
