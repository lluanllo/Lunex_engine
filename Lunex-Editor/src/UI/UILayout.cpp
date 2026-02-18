/**
 * @file UILayout.cpp
 * @brief Implementation of UI Layout Components
 */

#include "stpch.h"
#include "UILayout.h"

namespace Lunex::UI {

	// ============================================================================
	// PANEL & WINDOW COMPONENTS
	// ============================================================================
	
	bool BeginPanel(const std::string& title, bool* open, ImGuiWindowFlags flags) {
		ScopedColor colors({
			{ImGuiCol_WindowBg, Colors::BgMedium()},
			{ImGuiCol_ChildBg, Colors::BgMedium()},
			{ImGuiCol_Border, Colors::Border()},
			{ImGuiCol_TitleBg, Color::FromHex(0x111820)},
			{ImGuiCol_TitleBgActive, Color::FromHex(0x151D26)}
		});
		
		return ImGui::Begin(title.c_str(), open, flags);
	}
	
	void EndPanel() {
		ImGui::End();
	}
	
	bool BeginChild(const std::string& id, const Size& size, bool border, ImGuiWindowFlags flags) {
		return ImGui::BeginChild(id.c_str(), ToImVec2(size), border, flags);
	}
	
	void EndChild() {
		ImGui::EndChild();
	}
	
	bool BeginScrollArea(const std::string& id, const Size& size, bool horizontal) {
		ImGuiWindowFlags flags = 0;
		if (horizontal) {
			flags |= ImGuiWindowFlags_HorizontalScrollbar;
		}
		return ImGui::BeginChild(id.c_str(), ToImVec2(size), false, flags);
	}
	
	void EndScrollArea() {
		ImGui::EndChild();
	}
	
	// ============================================================================
	// CARD COMPONENT
	// ============================================================================
	
	bool BeginCard(const std::string& id, const Size& size, const CardStyle& style) {
		ScopedID scopedID(id);
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 cardSize = ToImVec2(size);
		
		if (cardSize.x <= 0) cardSize.x = ImGui::GetContentRegionAvail().x;
		if (cardSize.y <= 0) cardSize.y = 100.0f; // Default height
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Shadow
		if (style.shadow) {
			ImVec2 shadowMin = ImVec2(pos.x + style.shadowOffset.x, pos.y + style.shadowOffset.y);
			ImVec2 shadowMax = ImVec2(pos.x + cardSize.x + style.shadowOffset.x, pos.y + cardSize.y + style.shadowOffset.y);
			drawList->AddRectFilled(shadowMin, shadowMax, 
				ImGui::ColorConvertFloat4ToU32(ToImVec4(style.shadowColor)), 
				style.rounding);
		}
		
		// Card background
		ImVec2 max = ImVec2(pos.x + cardSize.x, pos.y + cardSize.y);
		drawList->AddRectFilled(pos, max, 
			ImGui::ColorConvertFloat4ToU32(ToImVec4(style.backgroundColor)), 
			style.rounding);
		
		// Border
		if (style.borderColor.a > 0) {
			drawList->AddRect(pos, max, 
				ImGui::ColorConvertFloat4ToU32(ToImVec4(style.borderColor)), 
				style.rounding);
		}
		
		// Begin child for content
		ImGui::SetCursorScreenPos(ImVec2(pos.x + style.padding, pos.y + style.padding));
		
		ImVec2 contentSize = ImVec2(
			cardSize.x - style.padding * 2,
			cardSize.y - style.padding * 2
		);
		
		ScopedColor childBg(ImGuiCol_ChildBg, Color(0, 0, 0, 0));
		return ImGui::BeginChild((id + "_content").c_str(), contentSize, false, 
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	}
	
	void EndCard() {
		ImGui::EndChild();
	}
	
	void Card(const std::string& id, const Size& size, const std::function<void()>& content, const CardStyle& style) {
		if (BeginCard(id, size, style)) {
			content();
		}
		EndCard();
	}
	
	// ============================================================================
	// SECTION & COLLAPSING HEADERS
	// ============================================================================
	
	void SectionHeader(const char* icon, const std::string& title) {
		ImGui::Spacing();
		ScopedColor textColor(ImGuiCol_Text, Colors::TextPrimary());
		ImGui::Text("%s  %s", icon, title.c_str());
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}
	
	bool BeginSection(const std::string& title, bool defaultOpen, const char* icon) {
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen * defaultOpen |
								   ImGuiTreeNodeFlags_Framed |
								   ImGuiTreeNodeFlags_SpanAvailWidth |
								   ImGuiTreeNodeFlags_FramePadding;
		
		ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		ScopedColor colors({
			{ImGuiCol_Header, Color::FromHex(0x1A2028)},
			{ImGuiCol_HeaderHovered, Color::FromHex(0x212830)},
			{ImGuiCol_HeaderActive, Color::FromHex(0x252D38)}
		});
		
		std::string displayTitle = icon ? (std::string(icon) + " " + title) : title;
		
		bool opened = ImGui::TreeNodeEx(title.c_str(), flags, "%s", displayTitle.c_str());
		
		if (opened) {
			ImGui::Indent(SpacingValues::SectionIndent);
		}
		
		return opened;
	}
	
	void EndSection() {
		ImGui::Unindent(SpacingValues::SectionIndent);
		ImGui::TreePop();
	}
	
	ComponentSectionResult BeginComponentSection(const std::string& title, bool canRemove) {
		ComponentSectionResult result;
		
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
								   ImGuiTreeNodeFlags_Framed |
								   ImGuiTreeNodeFlags_SpanAvailWidth |
								   ImGuiTreeNodeFlags_AllowOverlap |
								   ImGuiTreeNodeFlags_FramePadding;
		
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		
		ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		
		ScopedColor colors({
			{ImGuiCol_Header, Color::FromHex(0x1A2028)},
			{ImGuiCol_HeaderHovered, Color::FromHex(0x212830)},
			{ImGuiCol_HeaderActive, Color::FromHex(0x252D38)}
		});
		
		result.isOpen = ImGui::TreeNodeEx((void*)title.c_str(), flags, "%s", title.c_str());
		
		// Settings button
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		
		ScopedID buttonID(title.c_str());
		ScopedColor buttonColors({
			{ImGuiCol_Button, Color(0.13f, 0.16f, 0.20f, 1.0f)},
			{ImGuiCol_ButtonHovered, Color(0.20f, 0.25f, 0.31f, 1.0f)},
			{ImGuiCol_ButtonActive, Color(0.10f, 0.13f, 0.16f, 1.0f)}
		});
		
		if (!canRemove) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}
		
		if (ImGui::Button("+", ImVec2(lineHeight, lineHeight))) {
			if (canRemove) {
				ImGui::OpenPopup("ComponentSettings");
			}
		}
		
		if (!canRemove) {
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("This component cannot be removed independently");
			}
		}
		
		if (canRemove && ImGui::BeginPopup("ComponentSettings")) {
			if (ImGui::MenuItem("Remove component")) {
				result.removeClicked = true;
			}
			ImGui::EndPopup();
		}
		
		if (result.isOpen) {
			ImGui::Indent(SpacingValues::SectionIndent);
		}
		
		return result;
	}
	
	void EndComponentSection() {
		ImGui::Unindent(SpacingValues::SectionIndent);
		ImGui::TreePop();
	}
	
	// ============================================================================
	// GRID & COLUMNS
	// ============================================================================
	
	void BeginPropertyGrid(float labelWidth) {
		ScopedStyle vars(
			ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f),
			ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f)
		);
	}
	
	void EndPropertyGrid() {
		// Style vars popped automatically by ScopedStyle
	}
	
	 void BeginColumns(int count, bool border) {
		ImGui::Columns(count, nullptr, border);
	}
	
	void NextColumn() {
		ImGui::NextColumn();
	}
	
	void SetColumnWidth(int column, float width) {
		ImGui::SetColumnWidth(column, width);
	}
	
	void EndColumns() {
		ImGui::Columns(1);
	}
	
	void BeginHorizontal(float spacing) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, ImGui::GetStyle().ItemSpacing.y));
	}
	
	void EndHorizontal() {
		ImGui::PopStyleVar();
	}
	
	void BeginVertical(float spacing) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, spacing));
	}
	
	void EndVertical() {
		ImGui::PopStyleVar();
	}
	
	// ============================================================================
	// SPACING & SEPARATORS
	// ============================================================================
	
	void AddSpacing(float amount) {
		ImGui::Dummy(ImVec2(0, amount));
	}
	
	void AddVerticalSpacing(float amount) {
		ImGui::Dummy(ImVec2(0, amount));
	}
	
	void Separator() {
		ImGui::Separator();
	}
	
	void SeparatorText(const std::string& text) {
		ImGui::SeparatorText(text.c_str());
	}
	
	void Indent(float amount) {
		ImGui::Indent(amount);
	}
	
	void Unindent(float amount) {
		ImGui::Unindent(amount);
	}
	
	void Dummy(const Size& size) {
		ImGui::Dummy(ToImVec2(size));
	}
	
	void SameLine(float offsetFromStart, float spacing) {
		ImGui::SameLine(offsetFromStart, spacing);
	}
	
	void NewLine() {
		ImGui::NewLine();
	}
	
	// ============================================================================
	// TABS
	// ============================================================================
	
	bool BeginTabBar(const std::string& id, ImGuiTabBarFlags flags) {
		return ImGui::BeginTabBar(id.c_str(), flags);
	}
	
	void EndTabBar() {
		ImGui::EndTabBar();
	}
	
	bool BeginTabItem(const std::string& label, bool* open, ImGuiTabItemFlags flags) {
		return ImGui::BeginTabItem(label.c_str(), open, flags);
	}
	
	void EndTabItem() {
		ImGui::EndTabItem();
	}
	
	bool TabButton(const std::string& label) {
		return ImGui::TabItemButton(label.c_str(), ImGuiTabItemFlags_Trailing);
	}
	
	// ============================================================================
	// TREE
	// ============================================================================
	
	bool TreeNode(const std::string& label, bool selected, bool hasChildren, Ref<Texture2D> icon) {
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
								   ImGuiTreeNodeFlags_SpanAvailWidth |
								   ImGuiTreeNodeFlags_FramePadding;
		
		if (!hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		
		if (selected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		
		bool opened = false;
		
		if (icon) {
			// Draw icon before tree node
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(cursorPos.y + 2.0f);
			
			ImGui::Image(
				(void*)(intptr_t)icon->GetRendererID(),
				ImVec2(SpacingValues::IconMD, SpacingValues::IconMD),
				ImVec2(0, 1), ImVec2(1, 0)
			);
			
			ImGui::SameLine();
			ImGui::SetCursorPosY(cursorPos.y);
		}
		
		opened = ImGui::TreeNodeEx(label.c_str(), flags);
		
		return opened;
	}
	
	void TreePop() {
		ImGui::TreePop();
	}
	
	bool TreeLeaf(const std::string& label, bool selected, Ref<Texture2D> icon) {
		return TreeNode(label, selected, false, icon);
	}
	
	// ============================================================================
	// POPUPS & MODALS
	// ============================================================================
	
	void OpenPopup(const std::string& id) {
		ImGui::OpenPopup(id.c_str());
	}
	
	bool BeginPopup(const std::string& id, ImGuiWindowFlags flags) {
		return ImGui::BeginPopup(id.c_str(), flags);
	}
	
	void EndPopup() {
		ImGui::EndPopup();
	}
	
	bool BeginContextMenu(const std::string& id) {
		if (id.empty()) {
			return ImGui::BeginPopupContextItem();
		}
		return ImGui::BeginPopupContextItem(id.c_str());
	}
	
	void EndContextMenu() {
		ImGui::EndPopup();
	}
	
	bool BeginModal(const std::string& title, bool* open, const Size& size, ImGuiWindowFlags flags) {
		CenterNextWindow();
		ImGui::SetNextWindowSize(ToImVec2(size), ImGuiCond_Appearing);
		
		return ImGui::BeginPopupModal(title.c_str(), open, flags);
	}
	
	void EndModal() {
		ImGui::EndPopup();
	}
	
	void CenterNextWindow() {
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}
	
	// ============================================================================
	// MENU
	// ============================================================================
	
	bool BeginMenuBar() {
		return ImGui::BeginMenuBar();
	}
	
	void EndMenuBar() {
		ImGui::EndMenuBar();
	}
	
	bool BeginMenu(const std::string& label, bool enabled) {
		return ImGui::BeginMenu(label.c_str(), enabled);
	}
	
	void EndMenu() {
		ImGui::EndMenu();
	}
	
	bool MenuItem(const std::string& label, const char* shortcut, bool selected, bool enabled) {
		return ImGui::MenuItem(label.c_str(), shortcut, selected, enabled);
	}
	
	bool MenuItemWithIcon(const std::string& label, const char* icon, const char* shortcut, bool selected, bool enabled) {
		std::string fullLabel = std::string(icon) + " " + label;
		return ImGui::MenuItem(fullLabel.c_str(), shortcut, selected, enabled);
	}
	
	// ============================================================================
	// WINDOW UTILITIES
	// ============================================================================

	void SetNextWindowSize(const Size& size) {
		ImGui::SetNextWindowSize(ToImVec2(size), ImGuiCond_FirstUseEver);
	}

	bool BeginWindow(const std::string& title, bool* open, ImGuiWindowFlags flags) {
		return ImGui::Begin(title.c_str(), open, flags);
	}

	void EndWindow() {
		ImGui::End();
	}

	Size GetContentRegionAvail() {
		return FromImVec2(ImGui::GetContentRegionAvail());
	}

	Position GetCursorPos() {
		return FromImVec2(ImGui::GetCursorPos());
	}

	void SetCursorPos(const Position& pos) {
		ImGui::SetCursorPos(ToImVec2(pos));
	}

	void SetCursorPosX(float x) {
		ImGui::SetCursorPosX(x);
	}

	void SetCursorPosY(float y) {
		ImGui::SetCursorPosY(y);
	}

	Size CalcTextSize(const std::string& text) {
		ImVec2 size = ImGui::CalcTextSize(text.c_str());
		return Size(size.x, size.y);
	}

	// ============================================================================
	// TOOLTIPS
	// ============================================================================
	
	void Tooltip(const std::string& text) {
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", text.c_str());
		}
	}
	
	bool BeginTooltip() {
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			return true;
		}
		return false;
	}
	
	void EndTooltip() {
		ImGui::EndTooltip();
	}
	
	void TooltipDelayed(const std::string& text, float delay) {
		if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delay) {
			ImGui::SetTooltip("%s", text.c_str());
		}
	}

} // namespace Lunex::UI
