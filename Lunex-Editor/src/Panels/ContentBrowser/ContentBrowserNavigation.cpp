#include "stpch.h"
#include "ContentBrowserNavigation.h"

namespace Lunex {

	void ContentBrowserNavigation::Initialize(const std::filesystem::path& baseDirectory) {
		m_BaseDirectory = baseDirectory;
		m_CurrentDirectory = baseDirectory;
		m_DirectoryHistory.clear();
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;
	}

	void ContentBrowserNavigation::NavigateTo(const std::filesystem::path& directory) {
		if (m_CurrentDirectory == directory) {
			return;
		}

		AddToHistory(directory);
		m_CurrentDirectory = directory;
	}

	void ContentBrowserNavigation::NavigateToParent() {
		if (m_CurrentDirectory == m_BaseDirectory) {
			return;
		}

		std::filesystem::path parentPath = m_CurrentDirectory.parent_path();
		NavigateTo(parentPath);
	}

	void ContentBrowserNavigation::NavigateBack() {
		if (CanGoBack()) {
			m_HistoryIndex--;
			m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
		}
	}

	void ContentBrowserNavigation::NavigateForward() {
		if (CanGoForward()) {
			m_HistoryIndex++;
			m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
		}
	}

	void ContentBrowserNavigation::Reset(const std::filesystem::path& newBaseDirectory) {
		Initialize(newBaseDirectory);
	}

	void ContentBrowserNavigation::AddToHistory(const std::filesystem::path& directory) {
		m_HistoryIndex++;

		// Remove any forward history when navigating to a new directory
		if (m_HistoryIndex < (int)m_DirectoryHistory.size()) {
			m_DirectoryHistory.erase(
				m_DirectoryHistory.begin() + m_HistoryIndex,
				m_DirectoryHistory.end()
			);
		}

		m_DirectoryHistory.push_back(directory);
	}

} // namespace Lunex
