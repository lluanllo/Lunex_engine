#include "stpch.h"
#include "Component.h"

namespace Lunex::UI {

	std::unordered_map<ImGuiID, std::any>& Detail::StateStore() {
		static std::unordered_map<ImGuiID, std::any> store;
		return store;
	}

	void ClearState(const char* key) {
		Detail::StateStore().erase(ImGui::GetID(key));
	}

	void Island(const char* id, const Slot& content, const ImVec2& size, bool childWindow) {
		ImGui::PushID(id);
		if (childWindow) {
			ImGui::BeginChild("##island", size, ImGuiChildFlags_None);
			if (content) content();
			ImGui::EndChild();
		}
		else if (content) {
			content();
		}
		ImGui::PopID();
	}

	namespace Layouts {

		void Panel(const PanelProps& props, const Slots& slots) {
			ImGui::PushID(props.Title.c_str());
			const ImGuiStyle& style = ImGui::GetStyle();

			// Header
			if (!props.Title.empty() || slots.Header) {
				ImVec2 p = ImGui::GetCursorScreenPos();
				float w = ImGui::GetContentRegionAvail().x;
				float h = props.HeaderHeight > 0.0f ? props.HeaderHeight : ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;
				ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(Colors::BgDark().ToImVec4()));
				ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + h - 1), ImVec2(p.x + w, p.y + h - 1), ImGui::GetColorU32(Colors::BorderLight().ToImVec4()));
				ImGui::SetCursorScreenPos(ImVec2(p.x + style.FramePadding.x, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f));
				if (!props.Title.empty()) {
					ImGui::TextColored(Colors::TextPrimary().ToImVec4(), "%s", props.Title.c_str());
					if (slots.Header) ImGui::SameLine();
				}
				if (slots.Header) slots.Header();
				ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
				ImGui::Dummy(ImVec2(0, 0));
			}

			// Body
			const float footer = slots.Footer ? props.FooterHeight : 0.0f;
			ImGuiWindowFlags flags = props.Scrollable ? ImGuiWindowFlags_None : (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			ImGui::BeginChild("##body", ImVec2(0, -footer), ImGuiChildFlags_None, flags);
			if (slots.Default) slots.Default();
			ImGui::EndChild();

			// Footer
			if (slots.Footer) {
				ImVec2 p = ImGui::GetCursorScreenPos();
				float w = ImGui::GetContentRegionAvail().x;
				ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + w, p.y), ImGui::GetColorU32(Colors::BorderLight().ToImVec4()));
				ImGui::SetCursorScreenPos(ImVec2(p.x + style.FramePadding.x, p.y + (footer - ImGui::GetFrameHeight()) * 0.5f));
				ImGui::BeginGroup();
				slots.Footer();
				ImGui::EndGroup();
			}
			ImGui::PopID();
		}

		void Split(const SplitProps& props, const Slots& slots) {
			ImGui::PushID(props.ID);
			float& ratio = props.Ratio ? *props.Ratio : UseState<float>("ratio", 0.3f);
			const float total = ImGui::GetContentRegionAvail().x;
			const float height = ImGui::GetContentRegionAvail().y;
			const float splitter = 4.0f;
			float left = std::clamp(total * ratio, props.MinLeft, std::max(props.MinLeft, total - props.MinRight - splitter));

			ImGui::BeginChild("##left", ImVec2(left, height));
			slots.Render("Left");
			ImGui::EndChild();
			ImGui::SameLine(0, 0);

			ImGui::InvisibleButton("##splitter", ImVec2(splitter, height));
			const bool active = ImGui::IsItemActive();
			if (ImGui::IsItemHovered() || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if (active && total > 0.0f) ratio = std::clamp((left + ImGui::GetIO().MouseDelta.x) / total, 0.05f, 0.95f);
			ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(a.x + 1.5f, a.y), ImVec2(b.x - 1.5f, b.y),
				ImGui::GetColorU32((active ? Colors::Primary() : Colors::BorderLight()).ToImVec4()));
			ImGui::SameLine(0, 0);

			ImGui::BeginChild("##right", ImVec2(0, height));
			slots.Render("Right");
			ImGui::EndChild();
			ImGui::PopID();
		}

		void Card(const CardProps& props, const Slots& slots) {
			ImGui::PushID(props.Title.c_str());
			ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::BgCard().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_Border, (props.Highlight ? Colors::Primary() : Colors::BorderLight()).ToImVec4());
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
			ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
			if (!props.Title.empty()) {
				ImGui::TextColored(Colors::TextPrimary().ToImVec4(), "%s", props.Title.c_str());
				if (!props.Subtitle.empty()) ImGui::TextColored(Colors::TextSecondary().ToImVec4(), "%s", props.Subtitle.c_str());
				ImGui::Dummy(ImVec2(0, 2));
			}
			if (slots.Default) slots.Default();
			ImGui::EndChild();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);
			ImGui::PopID();
		}
	}

} // namespace Lunex::UI
