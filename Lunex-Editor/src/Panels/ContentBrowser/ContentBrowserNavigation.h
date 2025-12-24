#pragma once

#include <filesystem>
#include <vector>

namespace Lunex {

	// ============================================================================
	// CONTENT BROWSER NAVIGATION - Handles directory history and navigation
	// ============================================================================
	class ContentBrowserNavigation {
	public:
		ContentBrowserNavigation() = default;
		~ContentBrowserNavigation() = default;

		// Initialize with base directory
		void Initialize(const std::filesystem::path& baseDirectory);

		// Navigation
		void NavigateTo(const std::filesystem::path& directory);
		void NavigateToParent();
		void NavigateBack();
		void NavigateForward();

		// State queries
		bool CanGoBack() const { return m_HistoryIndex > 0; }
		bool CanGoForward() const { return m_HistoryIndex < (int)m_DirectoryHistory.size() - 1; }
		bool IsAtRoot() const { return m_CurrentDirectory == m_BaseDirectory; }

		// Getters
		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		const std::filesystem::path& GetBaseDirectory() const { return m_BaseDirectory; }

		// Reset
		void Reset(const std::filesystem::path& newBaseDirectory);

	private:
		void AddToHistory(const std::filesystem::path& directory);

	private:
		std::filesystem::path m_BaseDirectory;
		std::filesystem::path m_CurrentDirectory;
		std::vector<std::filesystem::path> m_DirectoryHistory;
		int m_HistoryIndex = 0;
	};

} // namespace Lunex
