/**
 * @file NavigationBar.h
 * @brief Navigation Bar Component for Content Browser
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <filesystem>
#include <functional>

namespace Lunex::UI {

	// ============================================================================
	// NAVIGATION BAR COMPONENT
	// ============================================================================
	
	struct NavigationBarStyle {
		float height = 40.0f;
		float buttonSize = 22.0f;
		float searchWidth = 200.0f;
		Color backgroundColor = Color(0.10f, 0.10f, 0.11f, 1.0f);
		Color buttonColor = Color(0.16f, 0.16f, 0.17f, 1.0f);
		Color buttonHoverColor = Color(0.26f, 0.59f, 0.98f, 0.4f);
		Color buttonActiveColor = Color(0.26f, 0.59f, 0.98f, 0.6f);
		Color inputBgColor = Color(0.14f, 0.14f, 0.15f, 1.0f);
		Color searchBgColor = Color(0.18f, 0.18f, 0.19f, 1.0f);
		Color searchHoverColor = Color(0.22f, 0.22f, 0.23f, 1.0f);
		Color textColor = Color(0.80f, 0.80f, 0.82f, 1.0f);
		Color searchTextColor = Color(0.85f, 0.85f, 0.87f, 1.0f);
	};
	
	struct NavigationBarCallbacks {
		std::function<void()> onBackClicked;
		std::function<void()> onForwardClicked;
	};
	
	/**
	 * @class NavigationBar
	 * @brief Renders the navigation bar with back/forward buttons, path display, and search
	 */
	class NavigationBar {
	public:
		NavigationBar() = default;
		~NavigationBar() = default;
		
		/**
		 * @brief Render the navigation bar
		 * @param currentPath Current directory path
		 * @param canGoBack Whether back navigation is available
		 * @param canGoForward Whether forward navigation is available
		 * @param backIcon Back button icon
		 * @param forwardIcon Forward button icon
		 * @param searchBuffer Search text buffer
		 * @param searchBufferSize Size of search buffer
		 * @param callbacks Event callbacks
		 */
		void Render(const std::filesystem::path& currentPath,
					bool canGoBack,
					bool canGoForward,
					Ref<Texture2D> backIcon,
					Ref<Texture2D> forwardIcon,
					char* searchBuffer,
					size_t searchBufferSize,
					const NavigationBarCallbacks& callbacks);
		
		// Style configuration
		void SetStyle(const NavigationBarStyle& style) { m_Style = style; }
		NavigationBarStyle& GetStyle() { return m_Style; }
		const NavigationBarStyle& GetStyle() const { return m_Style; }
		
	private:
		NavigationBarStyle m_Style;
		
		void RenderNavigationButtons(bool canGoBack, bool canGoForward,
									 Ref<Texture2D> backIcon, Ref<Texture2D> forwardIcon,
									 const NavigationBarCallbacks& callbacks);
		void RenderPathDisplay(const std::filesystem::path& currentPath);
		void RenderSearchBar(char* buffer, size_t bufferSize);
		void RenderBottomShadow();
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	void RenderNavigationBar(const std::filesystem::path& currentPath,
							 bool canGoBack,
							 bool canGoForward,
							 Ref<Texture2D> backIcon,
							 Ref<Texture2D> forwardIcon,
							 char* searchBuffer,
							 size_t searchBufferSize,
							 const NavigationBarCallbacks& callbacks,
							 const NavigationBarStyle& style = NavigationBarStyle());

} // namespace Lunex::UI
