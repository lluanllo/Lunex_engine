#pragma once

#include <filesystem>
#include <set>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations for ImGui types
struct ImVec2;
struct ImRect;

namespace Lunex {

	// ============================================================================
	// CONTENT BROWSER SELECTION - Handles file/folder selection and clipboard
	// ============================================================================
	class ContentBrowserSelection {
	public:
		enum class ClipboardOperation { None, Copy, Cut };

		ContentBrowserSelection() = default;
		~ContentBrowserSelection() = default;

		// Selection management
		void Select(const std::filesystem::path& path);
		void AddToSelection(const std::filesystem::path& path);
		void RemoveFromSelection(const std::filesystem::path& path);
		void ToggleSelection(const std::filesystem::path& path);
		void SelectRange(const std::filesystem::path& from, const std::filesystem::path& to,
			const std::filesystem::path& currentDirectory);
		void SelectAll(const std::filesystem::path& currentDirectory, const std::string& searchFilter = "");
		void Clear();

		// Selection queries
		bool IsSelected(const std::filesystem::path& path) const;
		bool HasSelection() const { return !m_SelectedItems.empty(); }
		size_t GetSelectionCount() const { return m_SelectedItems.size(); }
		const std::set<std::string>& GetSelectedItems() const { return m_SelectedItems; }
		const std::filesystem::path& GetLastSelected() const { return m_LastSelectedItem; }

		// Rectangle selection
		void BeginRectangleSelection(float startX, float startY);
		void UpdateRectangleSelection(float currentX, float currentY);
		void EndRectangleSelection();
		bool IsRectangleSelecting() const { return m_IsSelecting; }
		void GetSelectionRectStart(float& x, float& y) const { x = m_SelectionStartX; y = m_SelectionStartY; }
		void GetSelectionRectEnd(float& x, float& y) const { x = m_SelectionEndX; y = m_SelectionEndY; }

		// Item bounds for rectangle selection (stores as floats)
		void RegisterItemBounds(const std::filesystem::path& path, float minX, float minY, float maxX, float maxY);
		void ClearItemBounds();
		void CheckRectangleIntersection();

		// Clipboard operations
		void CopySelection();
		void CutSelection();
		void Paste(const std::filesystem::path& targetDirectory);
		bool CanPaste() const;
		ClipboardOperation GetClipboardOperation() const { return m_ClipboardOperation; }

	private:
		struct ItemRect {
			float MinX, MinY, MaxX, MaxY;
		};

		std::set<std::string> m_SelectedItems;
		std::filesystem::path m_LastSelectedItem;

		// Rectangle selection state
		bool m_IsSelecting = false;
		float m_SelectionStartX = 0.0f;
		float m_SelectionStartY = 0.0f;
		float m_SelectionEndX = 0.0f;
		float m_SelectionEndY = 0.0f;
		std::unordered_map<std::string, ItemRect> m_ItemBounds;

		// Clipboard state
		ClipboardOperation m_ClipboardOperation = ClipboardOperation::None;
		std::vector<std::filesystem::path> m_ClipboardItems;
	};

} // namespace Lunex
