/**
 * @file LunexUI.h
 * @brief Lunex UI Framework - Main Include Header
 * 
 * Include this file to access all UI components and utilities.
 * 
 * @example
 * ```cpp
 * #include "UI/LunexUI.h"
 * 
 * void MyPanel::OnImGuiRender() {
 *     using namespace Lunex::UI;
 *     
 *     if (BeginPanel("My Panel")) {
 *         SectionHeader("??", "Settings");
 *         
 *         if (PropertyFloat("Speed", m_Speed, 0.1f, 0.0f, 100.0f)) {
 *             OnSpeedChanged();
 *         }
 *         
 *         if (Button("Apply", ButtonVariant::Primary)) {
 *             ApplySettings();
 *         }
 *     }
 *     EndPanel();
 * }
 * ```
 * 
 * ## Architecture Overview
 * 
 * The Lunex UI Framework is organized into several layers:
 * 
 * ### Core (UICore.h)
 * - Color definitions and utilities
 * - Spacing constants
 * - RAII helpers (ScopedColor, ScopedStyle, ScopedID)
 * - Type definitions
 * 
 * ### Components (UIComponents.h)
 * - Basic UI elements: Text, Button, Input, Slider, Checkbox
 * - Specialized controls: Vec3Control, PropertyFloat, ColorPicker
 * - Image and texture components
 * 
 * ### Layout (UILayout.h)
 * - Panel and window management
 * - Card containers
 * - Sections and collapsing headers
 * - Grid and column layouts
 * - Tabs, Trees, Menus, Popups
 * 
 * ### Drag & Drop (UIDragDrop.h)
 * - Drag source and drop target helpers
 * - Payload types for content browser, entities
 * - Drop zone widgets
 * 
 * ### Components (Components/)
 * - AssetCard: Content browser cards
 * - EntityNode: Hierarchy tree nodes
 * - MaterialPreview: Material editor preview
 * - ScriptEntry: Script component entries
 * - SearchBar: Search input
 * - Toolbar: Floating toolbars
 * - StatusBar: Bottom status bars
 * - Toast: Notifications
 * - Breadcrumb: Path navigation
 * - Dialog: Modal dialogs
 * 
 * ## Design Principles
 * 
 * 1. **Consistent Styling**: All components use a shared color palette
 *    and spacing constants for visual consistency.
 * 
 * 2. **Declarative API**: Components are designed to be used in a
 *    declarative style, similar to modern web frameworks.
 * 
 * 3. **RAII Safety**: Style changes are automatically reverted using
 *    scoped helpers, preventing style leaks.
 * 
 * 4. **Composability**: Complex widgets are built from simpler
 *    components, allowing for easy customization.
 * 
 * 5. **Type Safety**: Strong typing and enums for variants, sizes,
 *    and options provide compile-time safety.
 */

#pragma once

// Core definitions and utilities
#include "UICore.h"

// Basic UI components
#include "UIComponents.h"

// Layout and container components
#include "UILayout.h"

// Drag and drop system
#include "UIDragDrop.h"

// Complex components (modular)
#include "Components/Components.h"

namespace Lunex::UI {

	/**
	 * @brief Initialize the UI framework
	 * Call this once at application startup after ImGui is initialized.
	 */
	inline void Initialize() {
		// Future: Load custom fonts, themes, etc.
	}
	
	/**
	 * @brief Shutdown the UI framework
	 * Call this before ImGui shutdown.
	 */
	inline void Shutdown() {
		// Future: Cleanup resources
	}
	
	/**
	 * @brief Begin a new frame
	 * Call this at the start of each frame before rendering UI.
	 */
	inline void BeginFrame() {
		// Future: Per-frame initialization
	}
	
	/**
	 * @brief End the current frame
	 * Call this at the end of each frame after rendering UI.
	 */
	inline void EndFrame() {
		// Render toast notifications
		RenderToasts();
	}

} // namespace Lunex::UI
