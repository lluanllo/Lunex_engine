/**
 * @file JobSystemPanel.cpp
 * @brief Job System Monitor Panel - Real-time performance monitoring
 * 
 * Features:
 * - Overview (total jobs, throughput, latency)
 * - Worker thread status visualization
 * - Queue status with progress bars
 * - Performance history graphs
 * - Test controls for stress testing
 */

#include "JobSystemPanel.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>
#include <chrono>
#include <thread>

namespace Lunex {

	// ============================================================================
	// CONSTRUCTOR
	// ============================================================================

	JobSystemPanel::JobSystemPanel() {
		m_ThroughputHistory.reserve(m_HistorySize);
		m_LatencyHistory.reserve(m_HistorySize);
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void JobSystemPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		using namespace UI;

		if (BeginPanel("Job System Monitor", &m_IsOpen)) {
			// Get current metrics
			JobMetricsSnapshot metrics = JobSystem::Get().GetMetrics();

			// Draw sections
			DrawOverview(metrics);
			Separator();
			DrawWorkerStatus(metrics);
			Separator();
			DrawQueueStatus(metrics);
			Separator();
			DrawPerformanceGraphs();
			Separator();
			DrawTestControls();
		}
		EndPanel();
	}

	// ============================================================================
	// OVERVIEW SECTION
	// ============================================================================

	void JobSystemPanel::DrawOverview(const JobMetricsSnapshot& metrics) {
		using namespace UI;

		if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();

			// Job counts
			uint64_t scheduled = metrics.TotalJobsScheduled;
			uint64_t completed = metrics.TotalJobsCompleted;
			uint64_t stolen = metrics.TotalJobsStolen;

			StatItem("Total Jobs Scheduled", static_cast<uint32_t>(scheduled));
			StatItem("Total Jobs Completed", static_cast<uint32_t>(completed));
			StatItem("Total Jobs Stolen", static_cast<uint32_t>(stolen));

			// Progress bar
			float progress = scheduled > 0 ? (float)completed / (float)scheduled : 1.0f;
			ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

			AddSpacing(SpacingValues::SM);

			// Throughput
			float throughput = metrics.Throughput;
			StatItem("Throughput", throughput, "%.1f jobs/sec");

			// Update throughput history
			if (m_ThroughputHistory.size() >= m_HistorySize) {
				m_ThroughputHistory.erase(m_ThroughputHistory.begin());
			}
			m_ThroughputHistory.push_back(throughput);

			// Latency
			float latency = metrics.AvgJobLatencyMs;
			StatItem("Avg Latency", latency, "%.2f ms");

			// Update latency history
			if (m_LatencyHistory.size() >= m_HistorySize) {
				m_LatencyHistory.erase(m_LatencyHistory.begin());
			}
			m_LatencyHistory.push_back(latency);

			// Work stealing efficiency
			if (completed > 0) {
				float stealRate = (float)stolen / (float)completed * 100.0f;
				StatItem("Work Steal Rate", stealRate, "%.1f%%");
			}

			Unindent();
		}
	}

	// ============================================================================
	// WORKER STATUS SECTION
	// ============================================================================

	void JobSystemPanel::DrawWorkerStatus(const JobMetricsSnapshot& metrics) {
		using namespace UI;

		if (ImGui::CollapsingHeader("Worker Threads", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();

			uint32_t active = metrics.ActiveWorkers;
			uint32_t idle = metrics.IdleWorkers;
			uint32_t total = active + idle;

			StatItem("Active Workers", static_cast<int>(active));
			StatItem("Idle Workers", static_cast<int>(idle));

			// Worker utilization bar
			if (total > 0) {
				float utilization = (float)active / (float)total;
				ImGui::ProgressBar(utilization, ImVec2(-1, 0), "");
				SameLine();
				
				char utilText[32];
				snprintf(utilText, sizeof(utilText), "%.0f%% Utilization", utilization * 100.0f);
				TextStyled(utilText, TextVariant::Muted);
			}

			AddSpacing(SpacingValues::SM);

			// Per-worker queue sizes
			TextStyled("Per-Worker Queue Sizes:", TextVariant::Secondary);
			for (size_t i = 0; i < 16; ++i) {
				uint32_t size = metrics.WorkerQueueSizes[i];
				if (size > 0 || i < 8) {
					char label[32];
					snprintf(label, sizeof(label), "Worker %zu", i);
					StatItem(label, static_cast<int>(size));
				}
			}

			Unindent();
		}
	}

	// ============================================================================
	// QUEUE STATUS SECTION
	// ============================================================================

	void JobSystemPanel::DrawQueueStatus(const JobMetricsSnapshot& metrics) {
		using namespace UI;

		if (ImGui::CollapsingHeader("Queues", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();

			uint32_t globalSize = metrics.GlobalQueueSize;
			uint32_t commandSize = metrics.CommandBufferSize;

			StatItem("Global Queue", static_cast<int>(globalSize));
			StatItem("Main-Thread Commands", static_cast<int>(commandSize));

			AddSpacing(SpacingValues::SM);

			// Global queue bar
			TextStyled("Global Queue:", TextVariant::Secondary);
			float globalProgress = std::min(1.0f, (float)globalSize / 100.0f);
			ImGui::ProgressBar(globalProgress, ImVec2(-1, 0), "");

			// Command buffer bar
			TextStyled("Command Buffer:", TextVariant::Secondary);
			float commandProgress = std::min(1.0f, (float)commandSize / 50.0f);
			ImGui::ProgressBar(commandProgress, ImVec2(-1, 0), "");

			Unindent();
		}
	}

	// ============================================================================
	// PERFORMANCE GRAPHS SECTION
	// ============================================================================

	void JobSystemPanel::DrawPerformanceGraphs() {
		using namespace UI;

		if (ImGui::CollapsingHeader("Performance History", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();

			// Throughput graph
			TextStyled("Throughput (jobs/sec):", TextVariant::Secondary);
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

			AddSpacing(SpacingValues::SM);

			// Latency graph
			TextStyled("Latency (ms):", TextVariant::Secondary);
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

			Unindent();
		}
	}

	// ============================================================================
	// TEST CONTROLS SECTION
	// ============================================================================

	void JobSystemPanel::DrawTestControls() {
		using namespace UI;

		if (ImGui::CollapsingHeader("Test Controls")) {
			Indent();

			TextStyled("Stress Test:", TextVariant::Secondary);
			ImGui::SliderInt("Job Count", &m_TestJobCount, 10, 10000);
			ImGui::SliderInt("Job Duration (ms)", &m_TestJobDuration, 0, 100);

			AddSpacing(SpacingValues::SM);

			if (Button("Run Test Jobs", ButtonVariant::Primary, ButtonSize::Medium, Size(-1, 0))) {
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

			AddSpacing(SpacingValues::SM);

			if (Button("Reset Metrics", ButtonVariant::Warning, ButtonSize::Medium, Size(-1, 0))) {
				JobSystem::Get().ResetMetrics();
				m_ThroughputHistory.clear();
				m_LatencyHistory.clear();
				LNX_LOG_INFO("Metrics reset");
			}

			Unindent();
		}
	}

} // namespace Lunex
