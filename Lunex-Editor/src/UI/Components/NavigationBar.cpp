/**
 * @file NavigationBar.cpp
 * @brief Navigation Bar Component Implementation
 */

#include "stpch.h"
#include "NavigationBar.h"
#include "../UILayout.h"

namespace Lunex::UI {

	void NavigationBar::Render(const std::filesystem::path& currentPath,
							   bool canGoBack,
							   bool canGoForward,
							   Ref<Texture2D> backIcon,
							   Ref<Texture2D> forwardIcon,
							   char* searchBuffer,
							   size_t searchBufferSize,
							   const NavigationBarCallbacks& callbacks) {
		ScopedColor bgColors({
			{ImGuiCol_ChildBg, m_Style.backgroundColor},
			{ImGuiCol_Button, m_Style.buttonColor},
			{ImGuiCol_ButtonHovered, m_Style.buttonHoverColor},
			{ImGuiCol_ButtonActive, m_Style.buttonActiveColor},
			{ImGuiCol_FrameBg, m_Style.buttonColor},
			{ImGuiCol_FrameBgHovered, Color(0.14f, 0.14f, 0.14f, 1.0f)},
			{ImGuiCol_Border, Color(0.055f, 0.055f, 0.055f, 1.0f)}
		});
		
		ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		
		ImGui::BeginChild("TopBar", ImVec2(0, m_Style.height), true, ImGuiWindowFlags_NoScrollbar);
		
		RenderNavigationButtons(canGoBack, canGoForward, backIcon, forwardIcon, callbacks);
		
		SameLine();
		Dummy(Size(16, 0));
		SameLine();
		
		RenderPathDisplay(currentPath);
		
		SameLine();
		
		RenderSearchBar(searchBuffer, searchBufferSize);
		
		ImGui::EndChild();
		
		RenderBottomShadow();
	}
	
	void NavigationBar::RenderNavigationButtons(bool canGoBack, bool canGoForward,
												Ref<Texture2D> backIcon, Ref<Texture2D> forwardIcon,
												const NavigationBarCallbacks& callbacks) {
		// Back button
		if (!canGoBack) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		}
		
		bool backClicked = false;
		if (backIcon && backIcon->GetRendererID() != 0) {
			backClicked = ImGui::ImageButton("##BackButton",
				(ImTextureID)(intptr_t)backIcon->GetRendererID(),
				ImVec2(m_Style.buttonSize, m_Style.buttonSize),
				ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0));
		} else {
			backClicked = ImGui::Button("<", ImVec2(30, 30));
		}
		
		if (backClicked && canGoBack && callbacks.onBackClicked) {
			callbacks.onBackClicked();
		}
		
		if (!canGoBack) {
			ImGui::PopStyleVar();
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Back");
		}
		
		SameLine();
		
		// Forward button
		if (!canGoForward) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		}
		
		bool forwardClicked = false;
		if (forwardIcon && forwardIcon->GetRendererID() != 0) {
			forwardClicked = ImGui::ImageButton("##ForwardButton",
				(ImTextureID)(intptr_t)forwardIcon->GetRendererID(),
				ImVec2(m_Style.buttonSize, m_Style.buttonSize),
				ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0));
		} else {
			forwardClicked = ImGui::Button(">", ImVec2(30, 30));
		}
		
		if (forwardClicked && canGoForward && callbacks.onForwardClicked) {
			callbacks.onForwardClicked();
		}
		
		if (!canGoForward) {
			ImGui::PopStyleVar();
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Forward");
		}
	}
	
	void NavigationBar::RenderPathDisplay(const std::filesystem::path& currentPath) {
		ScopedColor pathColors({
			{ImGuiCol_Text, m_Style.textColor},
			{ImGuiCol_FrameBg, m_Style.inputBgColor}
		});
		
		ImGui::AlignTextToFramePadding();
		
		static char pathBuffer[512];
		std::string pathStr = currentPath.string();
		strncpy_s(pathBuffer, sizeof(pathBuffer), pathStr.c_str(), _TRUNCATE);
		
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - m_Style.searchWidth - 20);
		ImGui::InputText("##PathDisplay", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
	}
	
	void NavigationBar::RenderSearchBar(char* buffer, size_t bufferSize) {
		ScopedColor searchColors({
			{ImGuiCol_FrameBg, m_Style.searchBgColor},
			{ImGuiCol_FrameBgHovered, m_Style.searchHoverColor},
			{ImGuiCol_FrameBgActive, Colors::BorderFocus()},
			{ImGuiCol_Text, m_Style.searchTextColor}
		});
		
		ImGui::SetNextItemWidth(m_Style.searchWidth);
		ImGui::InputTextWithHint("##Search", "?? Search...", buffer, bufferSize);
	}
	
	void NavigationBar::RenderBottomShadow() {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 topbarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(topbarMax.x - ImGui::GetContentRegionAvail().x, topbarMax.y);
		
		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.35f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(shadowStart.x, shadowStart.y + i),
				ImVec2(topbarMax.x, shadowStart.y + i + 1),
				shadowColor
			);
		}
	}
	
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
							 const NavigationBarStyle& style) {
		NavigationBar navBar;
		navBar.SetStyle(style);
		navBar.Render(currentPath, canGoBack, canGoForward, backIcon, forwardIcon,
					  searchBuffer, searchBufferSize, callbacks);
	}

} // namespace Lunex::UI
