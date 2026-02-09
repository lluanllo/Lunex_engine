/**
 * @file Breadcrumb.h
 * @brief Breadcrumb Navigation Component
 */

#pragma once

#include "../UICore.h"
#include <vector>

namespace Lunex::UI {

	// ============================================================================
	// BREADCRUMB COMPONENT
	// ============================================================================
	
	struct BreadcrumbItem {
		std::string label;
		std::string path;
	};
	
	struct BreadcrumbStyle {
		Color textColor = Color(0.55f, 0.55f, 0.55f, 1.0f);
		Color activeTextColor = Color(0.92f, 0.92f, 0.92f, 1.0f);
		Color separatorColor = Color(0.35f, 0.35f, 0.35f, 1.0f);
		const char* separator = ">";
	};
	
	/**
	 * @class Breadcrumb
	 * @brief Renders a breadcrumb navigation path
	 * 
	 * Features:
	 * - Clickable path segments
	 * - Separator customization
	 * - Active item highlighting
	 */
	class Breadcrumb {
	public:
		Breadcrumb() = default;
		~Breadcrumb() = default;
		
		/**
		 * @brief Render the breadcrumb
		 * @param id Unique identifier
		 * @param items Breadcrumb items
		 * @return Index of clicked item, -1 if none
		 */
		int Render(const std::string& id, const std::vector<BreadcrumbItem>& items);
		
		// Style configuration
		void SetStyle(const BreadcrumbStyle& style) { m_Style = style; }
		BreadcrumbStyle& GetStyle() { return m_Style; }
		const BreadcrumbStyle& GetStyle() const { return m_Style; }
		
	private:
		BreadcrumbStyle m_Style;
		
		void RenderSeparator();
		bool RenderItem(const BreadcrumbItem& item, bool isLast);
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	int RenderBreadcrumb(const std::string& id,
						 const std::vector<BreadcrumbItem>& items);

} // namespace Lunex::UI
