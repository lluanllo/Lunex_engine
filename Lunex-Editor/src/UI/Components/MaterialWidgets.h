/**
 * @file MaterialWidgets.h
 * @brief Lunex UI Framework - Material Editor Widgets
 *
 * Specialized UI components for material editing:
 * - CollapsibleSection: Accent-bar collapsible header
 * - MaterialTextureSlot: Compact drag-drop texture slot
 * - MaterialNameBar: Top header bar with name and save
 * - StatusBadge: Small status indicator
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <functional>
#include <filesystem>

namespace Lunex::UI {

	// ============================================================================
	// COLLAPSIBLE SECTION WITH ACCENT BAR
	// ============================================================================

	struct CollapsibleSectionStyle {
		Color BgNormal      = Color(0.17f, 0.18f, 0.20f, 1.0f);
		Color BgHovered     = Color(0.20f, 0.21f, 0.24f, 1.0f);
		Color BorderColor   = Color(0.18f, 0.18f, 0.21f, 1.0f);
		Color ArrowColor    = Color(0.50f, 0.50f, 0.55f, 1.0f);
		Color TextColor     = Color(0.90f, 0.90f, 0.92f, 1.0f);
		float Height        = 28.0f;
		float AccentWidth   = 3.0f;
	};

	/**
	 * @brief Collapsible section header with optional accent bar
	 * @param label Section title
	 * @param isOpen Reference to the open/closed state
	 * @param accentColor Optional accent bar color (nullptr = no accent)
	 * @param style Visual style options
	 * @return true if section is currently open
	 */
	bool CollapsibleSection(const char* label, bool& isOpen,
							const Color* accentColor = nullptr,
							const CollapsibleSectionStyle& style = {});

	// ============================================================================
	// MATERIAL TEXTURE SLOT
	// ============================================================================

	struct TextureSlotStyle {
		Color BgNormal      = Color(0.11f, 0.11f, 0.13f, 1.0f);
		Color BgHovered     = Color(0.14f, 0.14f, 0.16f, 1.0f);
		Color BorderColor   = Color(0.22f, 0.22f, 0.25f, 0.3f);
		Color AccentColor   = Color(0.26f, 0.59f, 0.98f, 0.4f);
		Color TextName      = Color(0.90f, 0.90f, 0.92f, 1.0f);
		Color TextInfo      = Color(0.45f, 0.45f, 0.50f, 1.0f);
		Color RemoveBg      = Color(0.55f, 0.20f, 0.20f, 1.0f);
		Color RemoveHover   = Color(0.70f, 0.25f, 0.25f, 1.0f);
		float Height        = 56.0f;
		float ThumbnailSize = 48.0f;
		float Rounding      = 4.0f;
		float Padding       = 6.0f;
	};

	/**
	 * @brief Compact texture slot with thumbnail, info, remove button and drag-drop
	 * @param label Slot label (e.g. "Albedo Map")
	 * @param texture Currently assigned texture (can be null)
	 * @param path Path to the texture file
	 * @param onTextureSet Callback when a new texture is dropped
	 * @param onTextureClear Callback when remove is clicked
	 * @param style Visual style options
	 */
	void MaterialTextureSlot(const std::string& label,
							 Ref<Texture2D> texture,
							 const std::string& path,
							 std::function<void(Ref<Texture2D>)> onTextureSet,
							 std::function<void()> onTextureClear,
							 const TextureSlotStyle& style = {});

	// ============================================================================
	// MATERIAL NAME BAR
	// ============================================================================

	struct NameBarStyle {
		Color BgColor       = Color(0.14f, 0.14f, 0.16f, 1.0f);
		Color BorderColor   = Color(0.22f, 0.22f, 0.25f, 1.0f);
		Color AccentColor   = Color(0.26f, 0.59f, 0.98f, 1.0f);
		Color TextColor     = Color(0.90f, 0.90f, 0.92f, 1.0f);
		float Height        = 32.0f;
		float AccentWidth   = 3.0f;
	};

	/**
	 * @brief Draws a name bar header with optional save button
	 * @param name Material name to display
	 * @param hasUnsavedChanges Shows save button when true
	 * @return true if save button was clicked
	 */
	bool MaterialNameBar(const std::string& name, bool hasUnsavedChanges,
						 const NameBarStyle& style = {});

	// ============================================================================
	// STATUS BADGE
	// ============================================================================

	/**
	 * @brief Small inline status badge with color
	 */
	void StatusBadge(const std::string& text, const Color& color);

	// ============================================================================
	// SECTION CONTENT AREA
	// ============================================================================

	/**
	 * @brief Begin a section content area (auto-resize child with padding)
	 * @param id Unique ID for the child region
	 * @param bgColor Background color
	 * @return true if visible
	 */
	bool BeginSectionContent(const std::string& id,
							 const Color& bgColor = Color(0.15f, 0.15f, 0.17f, 1.0f));

	/**
	 * @brief End a section content area
	 */
	void EndSectionContent();

	// ============================================================================
	// INFO ROW
	// ============================================================================

	/**
	 * @brief Property-style read-only info row
	 * @param label The label text
	 * @param fmt Printf-style format string for the value
	 */
	void InfoRow(const char* label, const char* fmt, ...);

	// ============================================================================
	// DRAG DROP TARGET HELPER
	// ============================================================================

	/**
	 * @brief Check for drag-drop content browser texture payload
	 * @return Path to dropped file, or empty string if nothing dropped
	 */
	std::string AcceptTextureDragDrop();

} // namespace Lunex::UI
