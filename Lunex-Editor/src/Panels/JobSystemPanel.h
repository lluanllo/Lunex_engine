#pragma once

#include "Lunex.h"
#include "Core/JobSystem/JobSystem.h"
#include <imgui.h>
#include <vector>

namespace Lunex {

	/// <summary>
	/// ImGui panel for JobSystem performance monitoring.
	/// Displays real-time metrics, worker status, and queue visualizations.
	/// </summary>
	class JobSystemPanel {
	public:
		JobSystemPanel();
		~JobSystemPanel() = default;

		/// <summary>
		/// Render ImGui panel (call every frame).
		/// </summary>
		void OnImGuiRender();

		/// <summary>
		/// Toggle panel visibility.
		/// </summary>
		void Toggle() { m_IsOpen = !m_IsOpen; }

		/// <summary>
		/// Set panel visibility.
		/// </summary>
		void SetOpen(bool open) { m_IsOpen = open; }

		/// <summary>
		/// Check if panel is open.
		/// </summary>
		bool IsOpen() const { return m_IsOpen; }

	private:
		/// <summary>
		/// Draw overview section (totals, throughput).
		/// </summary>
		void DrawOverview(const JobMetricsSnapshot& metrics);

		/// <summary>
		/// Draw worker thread status.
		/// </summary>
		void DrawWorkerStatus(const JobMetricsSnapshot& metrics);

		/// <summary>
		/// Draw queue visualizations.
		/// </summary>
		void DrawQueueStatus(const JobMetricsSnapshot& metrics);

		/// <summary>
		/// Draw performance graphs (history).
		/// </summary>
		void DrawPerformanceGraphs();

		/// <summary>
		/// Draw test controls (for debugging).
		/// </summary>
		void DrawTestControls();

	private:
		bool m_IsOpen = true;

		// Performance history for graphs
		std::vector<float> m_ThroughputHistory;
		std::vector<float> m_LatencyHistory;
		size_t m_HistorySize = 100;

		// Test job parameters
		int m_TestJobCount = 100;
		int m_TestJobDuration = 1; // milliseconds
	};

} // namespace Lunex
