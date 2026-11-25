#include "stpch.h"
#include "TimelinePanel.h"

#include "Scene/Components.h"
#include "Assets/Animation/AnimationAsset.h"
#include "Project/Project.h"
#include "Resources/Mesh/Model.h"

#include "../UI/UICore.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Lunex {

	namespace {
		std::filesystem::path ResolveAnimPath(const std::string& stored) {
			std::filesystem::path p(stored);
			if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
			return p;
		}
	}

	Entity TimelinePanel::ResolveTarget() {
		if (!m_Scene) return {};
		if (m_Selected && m_Scene->IsEntityValid(m_Selected) && m_Selected.HasComponent<AnimatorComponent>())
			return m_Selected;
		auto view = m_Scene->GetAllEntitiesWith<AnimatorComponent>();
		for (auto e : view) return Entity(e, m_Scene.get());
		return {};
	}

	float TimelinePanel::GetClipDuration(Entity entity, AnimatorComponent& animator, std::vector<float>* keyTimes) {
		Ref<Model> model = entity.HasComponent<MeshComponent>() ? entity.GetComponent<MeshComponent>().MeshModel : nullptr;
		const int modelClips = (model && model->IsSkinned()) ? static_cast<int>(model->GetAnimations().size()) : 0;
		if (animator.ClipIndex < modelClips)
			return model->GetAnimations()[animator.ClipIndex]->GetDurationSeconds();

		const int assetIndex = animator.ClipIndex - modelClips;
		if (assetIndex >= 0 && assetIndex < static_cast<int>(animator.AnimationAssets.size())) {
			if (auto asset = AnimationAsset::GetOrLoad(ResolveAnimPath(animator.AnimationAssets[assetIndex]))) {
				m_FrameRate = asset->FrameRate > 0.0f ? asset->FrameRate : 30.0f;
				if (keyTimes)
					for (const auto& track : asset->GetTracks())
						for (const auto& k : track.Keys) keyTimes->push_back(k.Time);
				return asset->Duration;
			}
		}
		return 0.0f;
	}

	void TimelinePanel::DockAboveContentBrowser() {
		// Only the first time the window ever appears (afterwards imgui.ini remembers the layout)
		if (m_DockChecked) return;
		m_DockChecked = true;
		if (ImGui::FindWindowSettingsByID(ImHashStr("Timeline"))) return;
		ImGuiWindow* browser = ImGui::FindWindowByName("Content Browser");
		if (!browser || browser->DockId == 0) return;

		ImGuiID top = 0, bottom = 0;
		ImGui::DockBuilderSplitNode(browser->DockId, ImGuiDir_Up, 0.24f, &top, &bottom);
		ImGui::DockBuilderDockWindow("Timeline", top);
		ImGui::DockBuilderFinish(browser->DockId);
	}

	void TimelinePanel::OnImGuiRender() {
		if (!m_Open) return;
		DockAboveContentBrowser();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
		if (!ImGui::Begin("Timeline", &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			ImGui::End();
			ImGui::PopStyleVar();
			return;
		}
		ImGui::PopStyleVar();

		Entity target = ResolveTarget();
		if (!target) {
			ImGui::TextDisabled("No animated entity in the scene (add an Animator to a rigged mesh).");
			ImGui::End();
			return;
		}

		auto& animator = target.GetComponent<AnimatorComponent>();
		std::vector<float> keyTimes;
		const float duration = std::max(GetClipDuration(target, animator, &keyTimes), 0.001f);
		const float fps = m_FrameRate;
		const int lastFrame = std::max(1, static_cast<int>(std::round(duration * fps)));
		bool& playing = m_Runtime ? animator.Playing : animator.PreviewInEditor;
		auto setTime = [&](float t) {
			animator.Time = std::clamp(t, 0.0f, duration);
			if (!m_Runtime) animator.PreviewInEditor = false;   // scrubbing pauses
			else animator.Playing = false;
		};

		// ---------------- Transport bar ----------------
		const float bh = ImGui::GetFrameHeight();
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(UI::Colors::Primary().ToImVec4(), "%s", target.GetComponent<TagComponent>().Tag.c_str());
		ImGui::SameLine();

		// Clip selector
		{
			Ref<Model> model = target.HasComponent<MeshComponent>() ? target.GetComponent<MeshComponent>().MeshModel : nullptr;
			std::vector<std::string> names;
			if (model && model->IsSkinned()) for (const auto& c : model->GetAnimations()) names.push_back(c->Name);
			for (const auto& a : animator.AnimationAssets) names.push_back(std::filesystem::path(a).stem().string());
			if (!names.empty()) {
				int index = std::clamp(animator.ClipIndex, 0, static_cast<int>(names.size()) - 1);
				ImGui::SetNextItemWidth(170.0f);
				if (ImGui::BeginCombo("##clip", names[index].c_str())) {
					for (int i = 0; i < static_cast<int>(names.size()); i++)
						if (ImGui::Selectable(names[i].c_str(), i == index)) {
							if (animator.Playing || animator.PreviewInEditor) animator.Play(i);
							else { animator.ClipIndex = i; animator.Time = 0.0f; }
						}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
			}
		}

		// Transport buttons (centered look: jump start, play/pause, stop, jump end)
		auto transport = [&](const char* id, const char* label, const char* tip) {
			const bool r = ImGui::Button((std::string(label) + "##" + id).c_str(), ImVec2(bh * 1.4f, bh));
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
			ImGui::SameLine(0, 2);
			return r;
		};
		if (transport("start", "|<", "Jump to start (Shift+Left)")) setTime(0.0f);
		if (transport("prevk", "<K", "Previous keyframe")) {
			float best = 0.0f;
			for (float k : keyTimes) if (k < animator.Time - 1e-4f) best = std::max(best, k);
			setTime(best);
		}
		ImGui::PushStyleColor(ImGuiCol_Button, playing ? UI::Colors::Primary().ToImVec4() : ImGui::GetStyleColorVec4(ImGuiCol_Button));
		if (transport("play", playing ? "||" : " >", "Play / Pause (Space in the timeline)")) playing = !playing;
		ImGui::PopStyleColor();
		if (transport("nextk", "K>", "Next keyframe")) {
			float best = duration;
			for (float k : keyTimes) if (k > animator.Time + 1e-4f) best = std::min(best, k);
			setTime(best);
		}
		if (transport("end", ">|", "Jump to end (Shift+Right)")) setTime(duration);
		if (transport("stop", "[]", "Stop and rewind")) { playing = false; animator.Time = 0.0f; }

		ImGui::SameLine(0, 12);
		int frame = static_cast<int>(std::round(std::fmod(animator.Time, duration) * fps));
		ImGui::SetNextItemWidth(70.0f);
		if (ImGui::DragInt("##frame", &frame, 0.25f, 0, lastFrame, "%d")) setTime(frame / fps);
		ImGui::SameLine();
		ImGui::TextDisabled("/ %d   %.2fs   %.0f fps", lastFrame, duration, fps);
		ImGui::SameLine(0, 16);
		ImGui::SetNextItemWidth(70.0f);
		ImGui::DragFloat("Speed", &animator.Speed, 0.01f, -5.0f, 5.0f, "%.2fx");
		ImGui::SameLine();
		ImGui::Checkbox("Loop", &animator.Loop);

		// Space toggles playback while the timeline is hovered/focused
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) playing = !playing;
			if (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) setTime(0.0f);
			if (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) setTime(duration);
			if (!ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) setTime(animator.Time - 1.0f / fps);
			if (!ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) setTime(animator.Time + 1.0f / fps);
		}

		// ---------------- Ruler + playhead ----------------
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float w = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
		const float h = std::max(ImGui::GetContentRegionAvail().y, 34.0f);
		const float margin = 10.0f;
		const float x0 = p.x + margin, x1 = p.x + w - margin;
		auto timeToX = [&](float t) { return x0 + (t / duration) * (x1 - x0); };

		ImGui::InvisibleButton("##ruler", ImVec2(w, h));
		if (ImGui::IsItemActive()) {
			const float t = (ImGui::GetMousePos().x - x0) / std::max(x1 - x0, 1.0f) * duration;
			setTime(std::round(t * fps) / fps);   // snap to frames
		}

		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(UI::Colors::BgDark().ToImVec4()), 3.0f);
		// Frame ticks: pick a step that keeps labels readable
		const float pxPerFrame = (x1 - x0) / lastFrame;
		int step = 1;
		for (int s : { 1, 2, 5, 10, 20, 50, 100, 200, 500 }) { step = s; if (s * pxPerFrame >= 48.0f) break; }
		const ImU32 tickCol = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.18f));
		const ImU32 labelCol = ImGui::GetColorU32(UI::Colors::TextMuted().ToImVec4());
		for (int f = 0; f <= lastFrame; f += step) {
			const float x = timeToX(f / fps);
			dl->AddLine(ImVec2(x, p.y + 16), ImVec2(x, p.y + h), tickCol);
			char buf[16]; snprintf(buf, sizeof(buf), "%d", f);
			dl->AddText(ImVec2(x + 3, p.y + 1), labelCol, buf);
		}
		// Keyframes (.luanim)
		const float ky = p.y + h * 0.62f;
		for (float k : keyTimes) {
			const float x = timeToX(k);
			dl->AddQuadFilled(ImVec2(x, ky - 5), ImVec2(x + 5, ky), ImVec2(x, ky + 5), ImVec2(x - 5, ky), IM_COL32(240, 190, 70, 255));
		}
		// Playhead
		const float cur = std::fmod(animator.Time, duration);
		const float hx = timeToX(cur);
		const ImU32 headCol = ImGui::GetColorU32(UI::Colors::Primary().ToImVec4());
		dl->AddLine(ImVec2(hx, p.y), ImVec2(hx, p.y + h), headCol, 2.0f);
		char cbuf[16]; snprintf(cbuf, sizeof(cbuf), "%d", static_cast<int>(std::round(cur * fps)));
		const ImVec2 ts = ImGui::CalcTextSize(cbuf);
		dl->AddRectFilled(ImVec2(hx - ts.x * 0.5f - 5, p.y), ImVec2(hx + ts.x * 0.5f + 5, p.y + ts.y + 2), headCol, 3.0f);
		dl->AddText(ImVec2(hx - ts.x * 0.5f, p.y + 1), IM_COL32(255, 255, 255, 255), cbuf);

		ImGui::End();
	}

} // namespace Lunex
