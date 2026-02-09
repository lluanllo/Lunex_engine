/**
 * @file StatusBar.cpp
 * @brief Status Bar Component Implementation
 */

#include "stpch.h"
#include "StatusBar.h"

namespace Lunex::UI {

	void StatusBar::Begin() {
		if (m_Style.showShadow) {
			RenderShadow();
		}
		
		ScopedColor colors({
			{ImGuiCol_ChildBg, m_Style.backgroundColor}
		});
		
		ImGui::BeginChild("##StatusBar", ImVec2(0, m_Style.height), false);
	}
	
	void StatusBar::End() {
		ImGui::EndChild();
	}
	
	void StatusBar::Item(const std::string& text, const char* icon) {
		if (icon) {
			ImGui::Text("%s %s", icon, text.c_str());
		} else {
			ImGui::Text("%s", text.c_str());
		}
	}
	
	bool StatusBar::Slider(const std::string& id, float& value, float min, float max, float width) {
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width - 10);
		ImGui::SetCursorPosY(6);
		ImGui::SetNextItemWidth(width);
		
		ScopedColor colors({
			{ImGuiCol_FrameBg, Color(0.10f, 0.10f, 0.10f, 1.0f)},
			{ImGuiCol_SliderGrab, Color(0.45f, 0.45f, 0.45f, 1.0f)},
			{ImGuiCol_SliderGrabActive, Colors::Primary()}
		});
		
		return ImGui::SliderFloat(("##" + id).c_str(), &value, min, max);
	}
	
	void StatusBar::RenderShadow() {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 bottomBarStart = ImGui::GetCursorScreenPos();
		
		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.3f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(bottomBarStart.x, bottomBarStart.y - i),
				ImVec2(bottomBarStart.x + ImGui::GetContentRegionAvail().x, bottomBarStart.y - i + 1),
				shadowColor
			);
		}
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTIONS
	// ============================================================================
	
	static StatusBar s_StatusBar;
	
	void BeginStatusBar(float height) {
		StatusBarStyle style;
		style.height = height;
		s_StatusBar.SetStyle(style);
		s_StatusBar.Begin();
	}
	
	void EndStatusBar() {
		s_StatusBar.End();
	}
	
	void StatusBarItem(const std::string& text, const char* icon) {
		s_StatusBar.Item(text, icon);
	}
	
	bool StatusBarSlider(const std::string& id, float& value, float min, float max, float width) {
		return s_StatusBar.Slider(id, value, min, max, width);
	}

} // namespace Lunex::UI
