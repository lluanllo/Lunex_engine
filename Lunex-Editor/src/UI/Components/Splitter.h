/**
 * @file Splitter.h
 * @brief Splitter Component for resizable panels
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// SPLITTER COMPONENT
	// ============================================================================
	
	struct SplitterStyle {
		float thickness = 4.0f;
		Color normalColor = Color(0, 0, 0, 0);
		Color hoverColor = Color(0.16f, 0.47f, 1.0f, 0.25f);
		Color activeColor = Color(0.16f, 0.47f, 1.0f, 0.45f);
	};
	
	/**
	 * @class Splitter
	 * @brief Renders a draggable splitter between panels
	 * 
	 * Features:
	 * - Horizontal or vertical splitting
	 * - Min/max constraints
	 * - Visual feedback on hover/drag
	 */
	class Splitter {
	public:
		Splitter() = default;
		~Splitter() = default;
		
		/**
		 * @brief Render a vertical splitter (resizes horizontal panels)
		 * @param id Unique identifier
		 * @param width Reference to the width being controlled
		 * @param minWidth Minimum width
		 * @param maxWidth Maximum width
		 * @param height Height of the splitter
		 * @return true if being dragged
		 */
		bool RenderVertical(const std::string& id,
							float& width,
							float minWidth,
							float maxWidth,
							float height);
		
		/**
		 * @brief Render a horizontal splitter (resizes vertical panels)
		 * @param id Unique identifier
		 * @param height Reference to the height being controlled
		 * @param minHeight Minimum height
		 * @param maxHeight Maximum height
		 * @param width Width of the splitter
		 * @return true if being dragged
		 */
		bool RenderHorizontal(const std::string& id,
							  float& height,
							  float minHeight,
							  float maxHeight,
							  float width);
		
		// Style configuration
		void SetStyle(const SplitterStyle& style) { m_Style = style; }
		SplitterStyle& GetStyle() { return m_Style; }
		const SplitterStyle& GetStyle() const { return m_Style; }
		
	private:
		SplitterStyle m_Style;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	bool VerticalSplitter(const std::string& id,
						  float& width,
						  float minWidth,
						  float maxWidth,
						  float height);
	
	bool HorizontalSplitter(const std::string& id,
							float& height,
							float minHeight,
							float maxHeight,
							float width);

} // namespace Lunex::UI
