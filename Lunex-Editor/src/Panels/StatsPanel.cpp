/**
 * @file StatsPanel.cpp
 * @brief Engine statistics: frame time, per-pass CPU/GPU cost, rendering, memory, scene.
 */

#include "stpch.h"
#include "StatsPanel.h"
#include "Scene/Components.h"
#include "Core/FrameProfiler.h"
#include "Core/JobSystem/JobSystem.h"
#include "RHI/RHI.h"
#include "RHI/RHIDevice.h"

#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	namespace {
		std::string Bytes(uint64_t b) {
			char buf[32];
			if (b >= (1ull << 30)) snprintf(buf, sizeof(buf), "%.2f GB", b / double(1ull << 30));
			else if (b >= (1ull << 20)) snprintf(buf, sizeof(buf), "%.1f MB", b / double(1ull << 20));
			else snprintf(buf, sizeof(buf), "%.1f KB", b / 1024.0);
			return buf;
		}

		std::string Count(uint64_t n) {
			char buf[32];
			if (n >= 1000000) snprintf(buf, sizeof(buf), "%.2fM", n / 1.0e6);
			else if (n >= 10000) snprintf(buf, sizeof(buf), "%.1fK", n / 1.0e3);
			else snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(n));
			return buf;
		}

		ImVec4 FrameColor(float ms) {
			if (ms <= 16.7f) return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);   // 60+ fps
			if (ms <= 33.4f) return ImVec4(0.95f, 0.78f, 0.30f, 1.0f);   // 30-60
			return ImVec4(0.95f, 0.38f, 0.30f, 1.0f);
		}

		bool Header(const char* title, bool defaultOpen = true) {
			ImGui::PushStyleColor(ImGuiCol_Header, UI::Colors::BgLight().ToImVec4());
			const bool open = ImGui::CollapsingHeader(title, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
			ImGui::PopStyleColor();
			return open;
		}

		void Row(const char* label, const std::string& value, const ImVec4* color = nullptr) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", label);
			ImGui::TableSetColumnIndex(1);
			if (color) ImGui::TextColored(*color, "%s", value.c_str());
			else ImGui::TextUnformatted(value.c_str());
		}

		bool BeginRows(const char* id) {
			if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
			ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthStretch, 0.55f);
			ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.45f);
			return true;
		}
	}

	void StatsPanel::OnImGuiRender() {
		if (!m_IsOpen) return;
		using namespace UI;

		if (BeginPanel("Stats", &m_IsOpen)) {
			DrawFrame();
			DrawTimings();
			DrawRendering();
			DrawMemory();
			DrawScene();

			if (Header("Hovered", false)) {
				const std::string name = m_HoveredEntity ? m_HoveredEntity.GetComponent<TagComponent>().Tag : "None";
				ImGui::TextUnformatted(name.c_str());
			}
		}
		EndPanel();
	}

	// ============================================================================
	// FRAME
	// ============================================================================

	void StatsPanel::DrawFrame() {
		const float avg = FrameProfiler::GetAverageMs();
		const float fps = avg > 0.0f ? 1000.0f / avg : 0.0f;

		// Big FPS readout
		ImGui::PushFont(nullptr);
		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextColored(FrameColor(avg), "%.0f FPS", fps);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopFont();
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("  %.2f ms   max %.1f   1%% low %.0f fps", avg, FrameProfiler::GetMaxMs(), FrameProfiler::GetOnePercentLowFps());

		// Frame time graph with 60 / 30 fps reference lines
		const auto& times = FrameProfiler::GetFrameTimes();
		const float w = ImGui::GetContentRegionAvail().x, h = 64.0f;
		const float top = std::max(40.0f, FrameProfiler::GetMaxMs() * 1.1f);
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(UI::Colors::BgDark().ToImVec4()), 3.0f);
		for (float ref : { 16.67f, 33.33f }) {
			const float y = p.y + h - (ref / top) * h;
			dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + w, y), IM_COL32(255, 255, 255, 40));
			dl->AddText(ImVec2(p.x + 3, y - 13), IM_COL32(255, 255, 255, 90), ref < 20.0f ? "60" : "30");
		}
		if (times.size() > 1) {
			const float step = w / static_cast<float>(std::max<size_t>(times.size() - 1, 1));
			for (size_t i = 1; i < times.size(); i++) {
				const ImVec2 a(p.x + step * (i - 1), p.y + h - std::min(times[i - 1] / top, 1.0f) * h);
				const ImVec2 b(p.x + step * i, p.y + h - std::min(times[i] / top, 1.0f) * h);
				dl->AddLine(a, b, ImGui::ColorConvertFloat4ToU32(FrameColor(times[i])), 1.5f);
			}
		}
		ImGui::Dummy(ImVec2(w, h + 4.0f));
		ImGui::TextDisabled("%s", FrameProfiler::GetGpuName());
	}

	// ============================================================================
	// WHERE THE TIME GOES
	// ============================================================================

	void StatsPanel::DrawTimings() {
		if (!Header("Frame breakdown (ms)")) return;
		ImGui::Checkbox("Sort by cost", &m_SortByCost);
		ImGui::SameLine();
		ImGui::TextDisabled("(GPU times are a few frames late)");

		auto sections = FrameProfiler::GetSections();
		if (sections.empty()) { ImGui::TextDisabled("No data yet"); return; }
		if (m_SortByCost)
			std::sort(sections.begin(), sections.end(), [](auto& a, auto& b) { return std::max(a.CpuMs, a.GpuMs) > std::max(b.CpuMs, b.GpuMs); });

		const float frame = std::max(FrameProfiler::GetAverageMs(), 0.001f);
		float worst = 0.0f;
		for (const auto& s : sections) if (s.Depth == 0 || m_SortByCost) worst = std::max(worst, std::max(s.CpuMs, s.GpuMs));

		if (ImGui::BeginTable("##timings", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 0.46f);
			ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthStretch, 0.14f);
			ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthStretch, 0.14f);
			ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_WidthStretch, 0.26f);
			ImGui::TableHeadersRow();
			for (const auto& s : sections) {
				const float cost = std::max(s.CpuMs, s.GpuMs);
				const bool hottest = cost == worst && cost > 0.5f;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const std::string indent(m_SortByCost ? 0 : s.Depth * 2, ' ');
				if (hottest) ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s%s", indent.c_str(), s.Name.c_str());
				else ImGui::Text("%s%s", indent.c_str(), s.Name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.2f", s.CpuMs);
				ImGui::TableSetColumnIndex(2);
				if (s.GpuMs >= 0.0f) ImGui::Text("%.2f", s.GpuMs); else ImGui::TextDisabled("-");
				ImGui::TableSetColumnIndex(3);
				const float frac = std::clamp(cost / frame, 0.0f, 1.0f);
				char label[16]; snprintf(label, sizeof(label), "%.0f%%", frac * 100.0f);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hottest ? ImVec4(0.95f, 0.55f, 0.25f, 1.0f) : UI::Colors::Primary().ToImVec4());
				ImGui::ProgressBar(frac, ImVec2(-1, ImGui::GetTextLineHeight()), label);
				ImGui::PopStyleColor();
			}
			ImGui::EndTable();
		}
	}

	// ============================================================================
	// RENDERING
	// ============================================================================

	void StatsPanel::DrawRendering() {
		if (!Header("Rendering")) return;
		const auto& c = FrameProfiler::GetRenderCounters();
		if (BeginRows("##render")) {
			Row("Draw calls (3D geometry)", Count(c.GeometryDrawCalls));
			Row("Draw calls (lighting)", Count(c.LightingDrawCalls));
			Row("Draw calls (2D / overlays)", Count(c.DrawCalls2D));
			Row("Meshes drawn", Count(c.Meshes));
			Row("Meshes culled", Count(c.CulledMeshes));
			Row("Triangles", Count(c.Triangles));
			Row("Quads (2D, sprites, particles)", Count(c.Quads2D));
			ImGui::EndTable();
		}
	}

	// ============================================================================
	// MEMORY
	// ============================================================================

	void StatsPanel::DrawMemory() {
		if (!Header("Memory")) return;
		if (BeginRows("##memory")) {
			Row("Process RAM (working set)", Bytes(FrameProfiler::GetProcessWorkingSet()));
			Row("Process RAM (private)", Bytes(FrameProfiler::GetProcessPrivateBytes()));
			uint32_t textures = 0; uint64_t textureBytes = 0;
			FrameProfiler::GetTextureStats(textures, textureBytes);
			Row("Textures (2D)", Count(textures) + "  /  " + Bytes(textureBytes));
			if (auto* device = RHI::RHIDevice::Get())
				Row("GPU resources tracked", Bytes(device->GetAllocatedMemory()));
			ImGui::EndTable();
		}
		uint64_t total = 0, available = 0;
		if (FrameProfiler::GetVideoMemory(total, available)) {
			if (total > 0) {
				const uint64_t used = total - available;
				const float frac = static_cast<float>(used) / static_cast<float>(total);
				const std::string label = "VRAM " + Bytes(used) + " / " + Bytes(total);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, frac > 0.85f ? ImVec4(0.95f, 0.38f, 0.30f, 1.0f) : UI::Colors::Primary().ToImVec4());
				ImGui::ProgressBar(frac, ImVec2(-1, 0), label.c_str());
				ImGui::PopStyleColor();
				ImGui::TextDisabled("Whole GPU (includes other applications)");
			}
			else {
				ImGui::TextDisabled("VRAM free: %s", Bytes(available).c_str());
			}
		}
		else {
			ImGui::TextDisabled("VRAM: driver does not report it (needs NVX/ATI memory info)");
		}
	}

	// ============================================================================
	// SCENE
	// ============================================================================

	void StatsPanel::DrawScene() {
		if (!Header("Scene") || !m_Scene) return;
		auto count = [](auto view) { uint64_t n = 0; for (auto e : view) { (void)e; n++; } return n; };
		uint64_t entities = 0;
		for (auto e : m_Scene->GetAllEntitiesWith<IDComponent>()) { (void)e; entities++; }
		uint64_t particles = 0, emitters = 0;
		for (auto e : m_Scene->GetAllEntitiesWith<ParticleSystemComponent>()) {
			particles += m_Scene->GetAllEntitiesWith<ParticleSystemComponent>().get<ParticleSystemComponent>(e).Particles.size();
			emitters++;
		}
		uint64_t meshes = 0, tris = 0;
		for (auto e : m_Scene->GetAllEntitiesWith<MeshComponent>()) {
			auto& mc = m_Scene->GetAllEntitiesWith<MeshComponent>().get<MeshComponent>(e);
			meshes++;
			if (mc.MeshModel) for (const auto& m : mc.MeshModel->GetMeshes()) tris += m->GetIndices().size() / 3;
		}
		if (BeginRows("##scene")) {
			Row("Entities", Count(entities));
			Row("Meshes / scene triangles", Count(meshes) + "  /  " + Count(tris));
			Row("Lights", Count(count(m_Scene->GetAllEntitiesWith<LightComponent>())));
			Row("Cameras", Count(count(m_Scene->GetAllEntitiesWith<CameraComponent>())));
			Row("Rigidbodies (3D / 2D)", Count(count(m_Scene->GetAllEntitiesWith<Rigidbody3DComponent>())) + "  /  " +
				Count(count(m_Scene->GetAllEntitiesWith<Rigidbody2DComponent>())));
			Row("Animators", Count(count(m_Scene->GetAllEntitiesWith<AnimatorComponent>())));
			Row("Particle emitters / alive", Count(emitters) + "  /  " + Count(particles));
			Row("Scripts / blueprints", Count(count(m_Scene->GetAllEntitiesWith<ScriptComponent>())) + "  /  " +
				Count(count(m_Scene->GetAllEntitiesWith<BlueprintComponent>())));
			Row("Job workers", Count(std::max(1u, std::thread::hardware_concurrency()) - 1));
			ImGui::EndTable();
		}
	}

} // namespace Lunex
