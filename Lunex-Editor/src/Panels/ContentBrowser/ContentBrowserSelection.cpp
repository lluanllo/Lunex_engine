#include "stpch.h"
#include "ContentBrowserSelection.h"

#include <algorithm>

namespace Lunex {

	void ContentBrowserSelection::Select(const std::filesystem::path& path) {
		Clear();
		AddToSelection(path);
	}

	void ContentBrowserSelection::AddToSelection(const std::filesystem::path& path) {
		m_SelectedItems.insert(path.string());
		m_LastSelectedItem = path;
	}

	void ContentBrowserSelection::RemoveFromSelection(const std::filesystem::path& path) {
		m_SelectedItems.erase(path.string());
	}

	void ContentBrowserSelection::ToggleSelection(const std::filesystem::path& path) {
		if (IsSelected(path)) {
			RemoveFromSelection(path);
		}
		else {
			AddToSelection(path);
		}
	}

	void ContentBrowserSelection::SelectRange(const std::filesystem::path& from,
		const std::filesystem::path& to, const std::filesystem::path& currentDirectory) {
		std::vector<std::filesystem::path> items;
		for (auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
			items.push_back(entry.path());
		}

		int fromIndex = -1;
		int toIndex = -1;
		for (int i = 0; i < (int)items.size(); i++) {
			if (items[i] == from) fromIndex = i;
			if (items[i] == to) toIndex = i;
		}

		if (fromIndex != -1 && toIndex != -1) {
			int start = (std::min)(fromIndex, toIndex);
			int end = (std::max)(fromIndex, toIndex);

			for (int i = start; i <= end; i++) {
				AddToSelection(items[i]);
			}
		}
	}

	void ContentBrowserSelection::SelectAll(const std::filesystem::path& currentDirectory,
		const std::string& searchFilter) {
		Clear();

		std::string searchLower = searchFilter;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		for (auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
			if (!searchFilter.empty()) {
				std::string filenameLower = entry.path().filename().string();
				std::transform(filenameLower.begin(), filenameLower.end(), 
					filenameLower.begin(), ::tolower);

				if (filenameLower.find(searchLower) == std::string::npos) {
					continue;
				}
			}

			AddToSelection(entry.path());
		}

		LNX_LOG_INFO("Selected all {0} items", m_SelectedItems.size());
	}

	void ContentBrowserSelection::Clear() {
		m_SelectedItems.clear();
		m_LastSelectedItem.clear();
	}

	bool ContentBrowserSelection::IsSelected(const std::filesystem::path& path) const {
		return m_SelectedItems.find(path.string()) != m_SelectedItems.end();
	}

	void ContentBrowserSelection::BeginRectangleSelection(float startX, float startY) {
		m_IsSelecting = true;
		m_SelectionStartX = startX;
		m_SelectionStartY = startY;
		m_SelectionEndX = startX;
		m_SelectionEndY = startY;
	}

	void ContentBrowserSelection::UpdateRectangleSelection(float currentX, float currentY) {
		if (m_IsSelecting) {
			m_SelectionEndX = currentX;
			m_SelectionEndY = currentY;
		}
	}

	void ContentBrowserSelection::EndRectangleSelection() {
		m_IsSelecting = false;
	}

	void ContentBrowserSelection::RegisterItemBounds(const std::filesystem::path& path, 
		float minX, float minY, float maxX, float maxY) {
		ItemRect rect{ minX, minY, maxX, maxY };
		m_ItemBounds[path.string()] = rect;
	}

	void ContentBrowserSelection::ClearItemBounds() {
		m_ItemBounds.clear();
	}

	void ContentBrowserSelection::CheckRectangleIntersection() {
		if (!m_IsSelecting) return;

		float selMinX = (std::min)(m_SelectionStartX, m_SelectionEndX);
		float selMinY = (std::min)(m_SelectionStartY, m_SelectionEndY);
		float selMaxX = (std::max)(m_SelectionStartX, m_SelectionEndX);
		float selMaxY = (std::max)(m_SelectionStartY, m_SelectionEndY);

		for (const auto& [pathStr, bounds] : m_ItemBounds) {
			// Check if rectangles overlap
			bool overlaps = !(bounds.MaxX < selMinX || bounds.MinX > selMaxX ||
				bounds.MaxY < selMinY || bounds.MinY > selMaxY);

			if (overlaps) {
				AddToSelection(std::filesystem::path(pathStr));
			}
		}
	}

	void ContentBrowserSelection::CopySelection() {
		m_ClipboardOperation = ClipboardOperation::Copy;
		m_ClipboardItems.clear();

		for (const auto& selectedPath : m_SelectedItems) {
			m_ClipboardItems.push_back(std::filesystem::path(selectedPath));
		}

		LNX_LOG_INFO("Copied {0} item(s) to clipboard", m_ClipboardItems.size());
	}

	void ContentBrowserSelection::CutSelection() {
		m_ClipboardOperation = ClipboardOperation::Cut;
		m_ClipboardItems.clear();

		for (const auto& selectedPath : m_SelectedItems) {
			m_ClipboardItems.push_back(std::filesystem::path(selectedPath));
		}

		LNX_LOG_INFO("Cut {0} item(s) to clipboard", m_ClipboardItems.size());
	}

	void ContentBrowserSelection::Paste(const std::filesystem::path& targetDirectory) {
		if (!CanPaste()) return;

		for (const auto& sourcePath : m_ClipboardItems) {
			try {
				std::filesystem::path destPath = targetDirectory / sourcePath.filename();

				// Handle name conflicts
				if (std::filesystem::exists(destPath)) {
					std::string baseName = sourcePath.stem().string();
					std::string extension = sourcePath.extension().string();
					int counter = 1;

					do {
						std::string newName = baseName + " (" + std::to_string(counter) + ")" + extension;
						destPath = targetDirectory / newName;
						counter++;
					} while (std::filesystem::exists(destPath));
				}

				if (m_ClipboardOperation == ClipboardOperation::Copy) {
					if (std::filesystem::is_directory(sourcePath)) {
						std::filesystem::copy(sourcePath, destPath,
							std::filesystem::copy_options::recursive);
					}
					else {
						std::filesystem::copy_file(sourcePath, destPath);
					}
					LNX_LOG_INFO("Copied {0} to {1}", sourcePath.filename().string(),
						destPath.filename().string());
				}
				else if (m_ClipboardOperation == ClipboardOperation::Cut) {
					std::filesystem::rename(sourcePath, destPath);
					LNX_LOG_INFO("Moved {0} to {1}", sourcePath.filename().string(),
						destPath.filename().string());
				}
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to paste {0}: {1}", sourcePath.filename().string(), e.what());
			}
		}

		// Clear clipboard after cut operation
		if (m_ClipboardOperation == ClipboardOperation::Cut) {
			m_ClipboardItems.clear();
			m_ClipboardOperation = ClipboardOperation::None;
		}

		Clear();
	}

	bool ContentBrowserSelection::CanPaste() const {
		return !m_ClipboardItems.empty() && m_ClipboardOperation != ClipboardOperation::None;
	}

} // namespace Lunex
