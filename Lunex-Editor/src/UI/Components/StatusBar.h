/**
 * @file StatusBar.h
 * @brief Status Bar Component
 */

#pragma once

#include "../UICore.h"

namespace Lunex::UI {

	// ============================================================================
	// STATUS BAR COMPONENT
	// ============================================================================
	
	struct StatusBarStyle {
		float height = 28.0f;
		Color backgroundColor = Color(0.09f, 0.09f, 0.09f, 1.0f);
		bool showShadow = true;
	};
	
	/**
	 * @class StatusBar
	 * @brief Renders a status bar at the bottom of a panel
	 * 
	 * Features:
	 * - Fixed height
	 * - Shadow effect
	 * - Status items
	 * - Sliders
	 */
	class StatusBar {
	public:
		StatusBar() = default;
		~StatusBar() = default;
		
		/**
		 * @brief Begin the status bar
		 */
		void Begin();
		
		/**
		 * @brief End the status bar
		 */
		void End();
		
		/**
		 * @brief Render a status item
		 * @param text Item text
		 * @param icon Optional icon
		 */
		void Item(const std::string& text, const char* icon = nullptr);
		
		/**
		 * @brief Render a slider in the status bar
		 * @param id Unique identifier
		 * @param value Slider value
		 * @param min Minimum value
		 * @param max Maximum value
		 * @param width Slider width
		 * @return true if value changed
		 */
		bool Slider(const std::string& id,
					float& value,
					float min,
					float max,
					float width = 160.0f);
		
		// Style configuration
		void SetStyle(const StatusBarStyle& style) { m_Style = style; }
		StatusBarStyle& GetStyle() { return m_Style; }
		const StatusBarStyle& GetStyle() const { return m_Style; }
		
	private:
		StatusBarStyle m_Style;
		
		void RenderShadow();
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	void BeginStatusBar(float height = 28.0f);
	void EndStatusBar();
	
	void StatusBarItem(const std::string& text, const char* icon = nullptr);
	
	bool StatusBarSlider(const std::string& id,
						 float& value,
						 float min,
						 float max,
						 float width = 160.0f);

} // namespace Lunex::UI
