/**
 * @file UILayout.h
 * @brief Lunex UI Framework - Layout Components
 * 
 * Provides layout containers and structural elements
 * for organizing UI content.
 */

#pragma once

#include "UICore.h"
#include <functional>

namespace Lunex::UI {

	// ============================================================================
	// PANEL & WINDOW COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Begin a styled panel (ImGui window with consistent styling)
	 * @return true if panel is visible
	 */
	bool BeginPanel(const std::string& title, bool* open = nullptr, ImGuiWindowFlags flags = 0);
	void EndPanel();
	
	/**
	 * @brief Begin a child region with consistent styling
	 */
	bool BeginChild(const std::string& id, 
					const Size& size = Size(0, 0),
					bool border = false,
					ImGuiWindowFlags flags = 0);
	void EndChild();
	
	/**
	 * @brief Scrollable content area
	 */
	bool BeginScrollArea(const std::string& id,
						 const Size& size = Size(0, 0),
						 bool horizontal = false);
	void EndScrollArea();
	
	// ============================================================================
	// CARD COMPONENT
	// ============================================================================
	
	struct CardStyle {
		Color backgroundColor = Colors::BgCard();
		Color borderColor = Colors::Border();
		float rounding = SpacingValues::CardRounding;
		float padding = SpacingValues::MD;
		bool shadow = true;
		Size shadowOffset = Size(2, 2);
		Color shadowColor = Color(0, 0, 0, 0.40f);
	};
	
	/**
	 * @brief Begin a card container (styled box with optional shadow)
	 */
	bool BeginCard(const std::string& id,
				   const Size& size = Size(0, 0),
				   const CardStyle& style = CardStyle());
	void EndCard();
	
	/**
	 * @brief Simple card that wraps content
	 */
	void Card(const std::string& id,
			  const Size& size,
			  const std::function<void()>& content,
			  const CardStyle& style = CardStyle());
	
	// ============================================================================
	// SECTION & COLLAPSING HEADERS
	// ============================================================================
	
	/**
	 * @brief Section header with icon and title
	 */
	void SectionHeader(const char* icon, const std::string& title);
	
	/**
	 * @brief Collapsing section (like CollapsingHeader but styled)
	 * @return true if section is expanded
	 */
	bool BeginSection(const std::string& title, 
					  bool defaultOpen = true,
					  const char* icon = nullptr);
	void EndSection();
	
	/**
	 * @brief Component section (for entity components in properties panel)
	 * Includes settings button and remove option
	 */
	struct ComponentSectionResult {
		bool isOpen = false;
		bool removeClicked = false;
		bool settingsClicked = false;
	};
	
	ComponentSectionResult BeginComponentSection(const std::string& title,
												 bool canRemove = true);
	void EndComponentSection();
	
	// ============================================================================
	// GRID & COLUMNS
	// ============================================================================
	
	/**
	 * @brief Begin a property grid layout (label : value columns)
	 */
	void BeginPropertyGrid(float labelWidth = SpacingValues::PropertyLabelWidth);
	void EndPropertyGrid();
	
	/**
	 * @brief Begin columns layout
	 */
	void BeginColumns(int count, bool border = false);
	void NextColumn();
	void SetColumnWidth(int column, float width);
	void EndColumns();
	
	/**
	 * @brief Begin a flex-like horizontal layout
	 */
	void BeginHorizontal(float spacing = SpacingValues::MD);
	void EndHorizontal();
	
	/**
	 * @brief Begin a flex-like vertical layout with spacing
	 */
	void BeginVertical(float spacing = SpacingValues::MD);
	void EndVertical();
	
	// ============================================================================
	// SPACING & SEPARATORS
	// ============================================================================
	
	void AddSpacing(float amount = SpacingValues::MD);
	void AddVerticalSpacing(float amount = SpacingValues::MD);
	
	void Separator();
	void SeparatorText(const std::string& text);
	
	void Indent(float amount = SpacingValues::SectionIndent);
	void Unindent(float amount = SpacingValues::SectionIndent);
	
	void Dummy(const Size& size);
	
	void SameLine(float offsetFromStart = 0.0f, float spacing = -1.0f);
	void NewLine();
	
	// ============================================================================
	// TABS
	// ============================================================================
	
	/**
	 * @brief Begin a tab bar
	 */
	bool BeginTabBar(const std::string& id, ImGuiTabBarFlags flags = 0);
	void EndTabBar();
	
	/**
	 * @brief Begin a tab item
	 * @param open Pointer to bool for closeable tabs, nullptr for non-closeable
	 * @return true if tab is selected
	 */
	bool BeginTabItem(const std::string& label, bool* open = nullptr, ImGuiTabItemFlags flags = 0);
	void EndTabItem();
	
	/**
	 * @brief Tab item button (for adding new tabs)
	 */
	bool TabButton(const std::string& label);
	
	// ============================================================================
	// TREE
	// ============================================================================
	
	/**
	 * @brief Tree node with consistent styling
	 */
	bool TreeNode(const std::string& label,
				  bool selected = false,
				  bool hasChildren = true,
				  Ref<Texture2D> icon = nullptr);
	void TreePop();
	
	/**
	 * @brief Tree node that doesn't push to stack (for leaf nodes)
	 */
	bool TreeLeaf(const std::string& label,
				  bool selected = false,
				  Ref<Texture2D> icon = nullptr);
	
	// ============================================================================
	// POPUPS & MODALS
	// ============================================================================
	
	/**
	 * @brief Open a popup
	 */
	void OpenPopup(const std::string& id);
	
	/**
	 * @brief Begin a popup
	 */
	bool BeginPopup(const std::string& id, ImGuiWindowFlags flags = 0);
	void EndPopup();
	
	/**
	 * @brief Begin a context menu popup
	 */
	bool BeginContextMenu(const std::string& id = "");
	void EndContextMenu();
	
	/**
	 * @brief Begin a modal dialog
	 */
	bool BeginModal(const std::string& title,
					bool* open = nullptr,
					const Size& size = Size(400, 300),
					ImGuiWindowFlags flags = 0);
	void EndModal();
	
	/**
	 * @brief Center the next window/modal
	 */
	void CenterNextWindow();
	
	// ============================================================================
	// MENU
	// ============================================================================
	
	bool BeginMenuBar();
	void EndMenuBar();
	
	bool BeginMenu(const std::string& label, bool enabled = true);
	void EndMenu();
	
	bool MenuItem(const std::string& label,
				  const char* shortcut = nullptr,
				  bool selected = false,
				  bool enabled = true);
	
	bool MenuItemWithIcon(const std::string& label,
						  const char* icon,
						  const char* shortcut = nullptr,
						  bool selected = false,
						  bool enabled = true);
	
	// ============================================================================
	// TOOLTIPS
	// ============================================================================
	
	/**
	 * @brief Show tooltip if item is hovered
	 */
	void Tooltip(const std::string& text);
	
	/**
	 * @brief Begin a custom tooltip (for complex content)
	 */
	bool BeginTooltip();
	void EndTooltip();
	
	/**
	 * @brief Show tooltip with delay
	 */
	void TooltipDelayed(const std::string& text, float delay = 0.5f);

} // namespace Lunex::UI
