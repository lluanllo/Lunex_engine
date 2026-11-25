#include "stpch.h"
#include "ProjectLauncherPanel.h"

#include "../UI/LunexUI.h"
#include "Log/Log.h"
#include "Project/ProjectThumbnail.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Lunex {

	namespace {
		ImVec4 Accent()      { return UI::Colors::Primary().ToImVec4(); }
		ImVec4 AccentHover() { return UI::Colors::PrimaryHover().ToImVec4(); }
		ImVec4 TextMuted()   { return UI::Colors::TextSecondary().ToImVec4(); }

		std::string FormatTime(std::time_t t) {
			if (t <= 0) return "";
			std::tm tm{};
			localtime_s(&tm, &t);
			char buf[64];
			std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tm);
			return buf;
		}

		bool ContainsCaseInsensitive(const std::string& haystack, const char* needle) {
			if (!needle || !*needle) return true;
			std::string h = haystack, n = needle;
			std::transform(h.begin(), h.end(), h.begin(), ::tolower);
			std::transform(n.begin(), n.end(), n.begin(), ::tolower);
			return h.find(n) != std::string::npos;
		}

		// Large accent-colored button used in the side bar / footer
		bool AccentButton(const char* label, const ImVec2& size) {
			ImGui::PushStyleColor(ImGuiCol_Button, Accent());
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentHover());
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::Colors::PrimaryActive().ToImVec4());
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
			bool pressed = ImGui::Button(label, size);
			ImGui::PopStyleColor(4);
			return pressed;
		}
	}

	// Recent entries are stored as "<unix time>|<path>"
	struct RecentLine { std::time_t Time = 0; std::filesystem::path Path; };

	static std::vector<RecentLine> s_RecentLines;

	ProjectLauncherPanel::ProjectLauncherPanel() {
		LoadRecent();
	}

	std::filesystem::path ProjectLauncherPanel::GetRecentFilePath() {
		std::filesystem::path base;
		char* appData = nullptr;
		size_t len = 0;
		if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) {
			base = std::filesystem::path(appData) / "Lunex";
			free(appData);
		}
		else {
			base = std::filesystem::current_path();
		}
		return base / "RecentProjects.txt";
	}

	void ProjectLauncherPanel::LoadRecent() {
		s_RecentLines.clear();
		std::ifstream in(GetRecentFilePath());
		std::string line;
		while (std::getline(in, line)) {
			if (line.empty()) continue;
			RecentLine r;
			auto sep = line.find('|');
			if (sep != std::string::npos) {
				r.Time = static_cast<std::time_t>(std::strtoll(line.substr(0, sep).c_str(), nullptr, 10));
				r.Path = line.substr(sep + 1);
			}
			else {
				r.Path = line;
			}
			s_RecentLines.push_back(r);
		}
		RefreshRecentInfo();
	}

	void ProjectLauncherPanel::SaveRecent() const {
		auto file = GetRecentFilePath();
		std::error_code ec;
		std::filesystem::create_directories(file.parent_path(), ec);
		std::ofstream out(file, std::ios::trunc);
		for (const auto& r : s_RecentLines) {
			out << static_cast<long long>(r.Time) << "|" << r.Path.string() << "\n";
		}
	}

	void ProjectLauncherPanel::RefreshRecentInfo() {
		m_Recent.clear();
		for (const auto& r : s_RecentLines) {
			RecentProject p;
			p.ProjectFile = r.Path;
			p.Exists = std::filesystem::exists(r.Path);
			// Project name = folder that contains the project file
			p.Name = r.Path.parent_path().filename().string();
			if (p.Name.empty()) p.Name = r.Path.stem().string();
			p.LastOpened = FormatTime(r.Time);
			m_Recent.push_back(p);
		}
		if (m_Selected >= static_cast<int>(m_Recent.size())) m_Selected = -1;
	}

	void ProjectLauncherPanel::AddRecentProject(const std::filesystem::path& projectFile) {
		std::error_code ec;
		std::filesystem::path abs = std::filesystem::weakly_canonical(projectFile, ec);
		if (ec) abs = projectFile;

		s_RecentLines.erase(std::remove_if(s_RecentLines.begin(), s_RecentLines.end(),
			[&](const RecentLine& r) {
				std::error_code e2;
				return std::filesystem::equivalent(r.Path, abs, e2) || r.Path == abs;
			}), s_RecentLines.end());

		RecentLine entry;
		entry.Time = std::time(nullptr);
		entry.Path = abs;
		s_RecentLines.insert(s_RecentLines.begin(), entry);
		if (s_RecentLines.size() > MaxRecent) s_RecentLines.resize(MaxRecent);

		SaveRecent();
		RefreshRecentInfo();
	}

	void ProjectLauncherPanel::RemoveRecent(size_t index) {
		if (index >= s_RecentLines.size()) return;
		s_RecentLines.erase(s_RecentLines.begin() + index);
		SaveRecent();
		RefreshRecentInfo();
		m_Selected = -1;
	}

	// ============================================================================
	// THUMBNAILS / VERSIONS
	// ============================================================================

	Ref<Texture2D> ProjectLauncherPanel::GetThumbnail(const std::filesystem::path& projectFile) {
		const auto path = ProjectThumbnail::GetPath(projectFile.parent_path());
		auto& entry = m_Thumbnails[path.string()];
		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) { entry = {}; entry.Missing = true; return nullptr; }
		auto time = std::filesystem::last_write_time(path, ec);
		if (!entry.Texture || entry.Time != time) {
			entry.Texture = Texture2D::Create(path.string());
			entry.Time = time;
		}
		return (entry.Texture && entry.Texture->IsLoaded()) ? entry.Texture : nullptr;
	}

	void ProjectLauncherPanel::RefreshVersions() {
		m_Versions.clear();
		m_VersionsProject.clear();
		if (m_Selected < 0 || m_Selected >= static_cast<int>(m_Recent.size()) || !m_Recent[m_Selected].Exists) return;
		m_VersionsProject = m_Recent[m_Selected].ProjectFile;
		m_Versions = ProjectVersioning::CreateProvider(m_VersionsProject)->List();
	}

	// ============================================================================
	// UI
	// ============================================================================

	void ProjectLauncherPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		ImGui::SetNextWindowViewport(vp->ID);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, UI::Colors::BgDark().ToImVec4());
		bool visible = ImGui::Begin("##ProjectLauncher", nullptr, flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		if (!visible) { ImGui::End(); return; }

		if (ImGui::IsWindowAppearing()) ImGui::SetWindowFocus();
		if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
			ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

		const float headerH = 64.0f;
		const float sidebarW = 230.0f;
		const float footerH = 56.0f;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 origin = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();

		// ---------------- Header ----------------
		dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + headerH),
			ImGui::GetColorU32(UI::Colors::BgDark().Darker(0.03f).ToImVec4()));
		dl->AddRectFilled(ImVec2(origin.x, origin.y + headerH - 2.0f), ImVec2(origin.x + size.x, origin.y + headerH),
			ImGui::GetColorU32(Accent()));
		ImGui::SetCursorPos(ImVec2(24.0f, 14.0f));
		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextUnformatted("LUNEX");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SameLine();
		ImGui::SetCursorPosY(24.0f);
		ImGui::TextColored(TextMuted(), "  Project Browser");

		// ---------------- Sidebar ----------------
		ImGui::SetCursorPos(ImVec2(0.0f, headerH));
		DrawSidebar(sidebarW, size.y - headerH);

		// ---------------- Content ----------------
		int openRequest = -1;
		int removeRequest = -1;
		const bool hasFooter = m_Section == Section::Projects;
		ImGui::SetCursorPos(ImVec2(sidebarW, headerH));
		ImGui::BeginChild("##LauncherContent", ImVec2(size.x - sidebarW, size.y - headerH - (hasFooter ? footerH : 0.0f)), false);
		switch (m_Section) {
			case Section::Projects: DrawProjectsSection(openRequest, removeRequest); break;
			case Section::Create:   DrawCreateSection(); break;
			case Section::Versions: DrawVersionsSection(); break;
		}
		ImGui::EndChild();

		// ---------------- Footer (projects) ----------------
		if (hasFooter) {
			ImGui::SetCursorPos(ImVec2(sidebarW, size.y - footerH));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::Colors::BgMedium().ToImVec4());
			ImGui::BeginChild("##LauncherFooter", ImVec2(size.x - sidebarW, footerH), false);
			ImGui::SetCursorPos(ImVec2(24.0f, 16.0f));
			if (m_Selected >= 0 && m_Selected < static_cast<int>(m_Recent.size()))
				ImGui::TextColored(TextMuted(), "%s", m_Recent[m_Selected].ProjectFile.string().c_str());

			float right = ImGui::GetWindowWidth() - 24.0f;
			ImGui::SetCursorPos(ImVec2(right - 150.0f - 8.0f - 190.0f, 12.0f));
			if (ImGui::Button("Continue without project", ImVec2(190.0f, 32.0f))) Close();
			ImGui::SetCursorPos(ImVec2(right - 150.0f, 12.0f));
			bool canOpen = m_Selected >= 0 && m_Selected < static_cast<int>(m_Recent.size()) && m_Recent[m_Selected].Exists;
			ImGui::BeginDisabled(!canOpen);
			if (AccentButton("Open", ImVec2(150.0f, 32.0f))) openRequest = m_Selected;
			ImGui::EndDisabled();
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		ImGui::End();

		// Deferred actions (after End so callbacks can safely open other windows)
		if (removeRequest >= 0) {
			RemoveRecent(static_cast<size_t>(removeRequest));
		}
		else if (openRequest >= 0 && openRequest < static_cast<int>(m_Recent.size()) && m_OnOpenProject) {
			auto path = m_Recent[openRequest].ProjectFile;
			m_OnOpenProject(path);
		}
	}

	void ProjectLauncherPanel::DrawSidebar(float width, float height) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::Colors::BgMedium().ToImVec4());
		ImGui::BeginChild("##LauncherSidebar", ImVec2(width, height), false);
		ImGui::SetCursorPos(ImVec2(16.0f, 20.0f));
		ImGui::BeginGroup();
		const float w = width - 32.0f;

		auto navItem = [&](const char* label, Section section) {
			const bool active = m_Section == section;
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::PushStyleColor(ImGuiCol_Button, active ? UI::Colors::BgHover().ToImVec4() : ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::Colors::BgLight().ToImVec4());
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (ImGui::Button(label, ImVec2(w, 32.0f))) {
				m_Section = section;
				if (section == Section::Versions) RefreshVersions();
				m_CreateError.clear();
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);
			if (active)
				ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 3.0f, p.y + 32.0f), ImGui::GetColorU32(Accent()));
		};

		ImGui::TextColored(TextMuted(), "LAUNCHER");
		ImGui::Dummy(ImVec2(0, 6));
		navItem("   Projects", Section::Projects);
		navItem("   New Project", Section::Create);
		navItem("   Versions", Section::Versions);

		ImGui::Dummy(ImVec2(0, 18));
		if (ImGui::Button("Open Project...", ImVec2(w, 32.0f)) && m_OnBrowseProject) m_OnBrowseProject();

		ImGui::Dummy(ImVec2(0, 24));
		ImGui::TextColored(TextMuted(), "RECENT");
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::Text("%d project%s", (int)m_Recent.size(), m_Recent.size() == 1 ? "" : "s");
		ImGui::EndGroup();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void ProjectLauncherPanel::DrawProjectsSection(int& openRequest, int& removeRequest) {
		ImGui::SetCursorPos(ImVec2(24.0f, 18.0f));
		ImGui::TextUnformatted("Recent Projects");
		ImGui::SameLine();
		ImGui::SetCursorPosX(std::max(200.0f, ImGui::GetWindowWidth() - 24.0f - 260.0f));
		ImGui::SetNextItemWidth(260.0f);
		ImGui::InputTextWithHint("##LauncherFilter", "Search projects...", m_Filter, sizeof(m_Filter));
		ImGui::Dummy(ImVec2(0, 8));

		const float cardW = 240.0f, thumbH = 135.0f, cardH = thumbH + 56.0f, gap = 16.0f;
		float avail = ImGui::GetContentRegionAvail().x - 24.0f;
		int columns = std::max(1, static_cast<int>((avail + gap) / (cardW + gap)));

		if (m_Recent.empty()) {
			ImGui::SetCursorPosX(24.0f);
			ImGui::TextColored(TextMuted(), "No recent projects yet. Create a new one or open an existing project.lunex.");
		}

		int shown = 0;
		for (int i = 0; i < static_cast<int>(m_Recent.size()); i++) {
			const auto& p = m_Recent[i];
			if (!ContainsCaseInsensitive(p.Name, m_Filter) && !ContainsCaseInsensitive(p.ProjectFile.string(), m_Filter))
				continue;

			if (shown % columns == 0) ImGui::SetCursorPosX(24.0f);
			else ImGui::SameLine(0.0f, gap);
			shown++;

			ImGui::PushID(i);
			ImVec2 cardMin = ImGui::GetCursorScreenPos();
			ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);
			bool clicked = ImGui::InvisibleButton("##card", ImVec2(cardW, cardH));
			bool hovered = ImGui::IsItemHovered();
			if (clicked) m_Selected = i;
			if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && p.Exists) openRequest = i;

			if (ImGui::BeginPopupContextItem("##cardctx")) {
				if (ImGui::MenuItem("Open", nullptr, false, p.Exists)) openRequest = i;
				if (ImGui::MenuItem("Versions...", nullptr, false, p.Exists)) { m_Selected = i; m_Section = Section::Versions; RefreshVersions(); }
				ImGui::Separator();
				if (ImGui::MenuItem("Remove from list")) removeRequest = i;
				ImGui::EndPopup();
			}
			if (hovered) ImGui::SetTooltip("%s", p.ProjectFile.string().c_str());

			bool selected = (m_Selected == i);
			ImVec4 bg = selected ? UI::Colors::BgHover().ToImVec4()
				: hovered ? UI::Colors::BgLight().Lighter(0.02f).ToImVec4()
				: UI::Colors::BgLight().ToImVec4();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(bg), 4.0f);

			// Thumbnail: captured from the project's camera, or the initial as fallback
			ImVec2 thumbMax(cardMax.x, cardMin.y + thumbH);
			dl->AddRectFilled(cardMin, thumbMax, ImGui::GetColorU32(UI::Colors::BgDark().ToImVec4()), 4.0f, ImDrawFlags_RoundCornersTop);
			Ref<Texture2D> thumb = p.Exists ? GetThumbnail(p.ProjectFile) : nullptr;
			if (thumb) {
				// Cover-fit the image into the thumbnail rect (texture rows are bottom-up)
				float imgAspect = static_cast<float>(thumb->GetWidth()) / std::max(1u, thumb->GetHeight());
				float boxAspect = cardW / thumbH;
				ImVec2 uv0(0, 0), uv1(1, 1);
				if (imgAspect > boxAspect) { float f = boxAspect / imgAspect; uv0.x = 0.5f - f * 0.5f; uv1.x = 0.5f + f * 0.5f; }
				else { float f = imgAspect / boxAspect; uv0.y = 0.5f - f * 0.5f; uv1.y = 0.5f + f * 0.5f; }
				dl->AddImageRounded((ImTextureID)(intptr_t)thumb->GetRendererID(), cardMin, thumbMax,
					ImVec2(uv0.x, 1.0f - uv0.y), ImVec2(uv1.x, 1.0f - uv1.y), IM_COL32_WHITE, 4.0f, ImDrawFlags_RoundCornersTop);
			}
			else {
				char initial[2] = { p.Name.empty() ? '?' : static_cast<char>(::toupper(static_cast<unsigned char>(p.Name[0]))), 0 };
				ImFont* font = ImGui::GetFont();
				float big = ImGui::GetFontSize() * 2.6f;
				ImVec2 ts = font->CalcTextSizeA(big, FLT_MAX, 0.0f, initial);
				dl->AddText(font, big, ImVec2((cardMin.x + cardMax.x - ts.x) * 0.5f, (cardMin.y + thumbMax.y - ts.y) * 0.5f),
					ImGui::GetColorU32(p.Exists ? Accent() : TextMuted()), initial);
			}
			if (selected) dl->AddRect(cardMin, cardMax, ImGui::GetColorU32(Accent()), 4.0f, 0, 2.0f);

			ImVec2 textPos(cardMin.x + 10.0f, thumbMax.y + 8.0f);
			dl->PushClipRect(cardMin, cardMax, true);
			dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), p.Name.c_str());
			std::string sub = p.Exists ? (p.LastOpened.empty() ? "" : "Opened " + p.LastOpened) : "Missing on disk";
			dl->AddText(ImVec2(textPos.x, textPos.y + 20.0f), ImGui::GetColorU32(TextMuted()), sub.c_str());
			dl->PopClipRect();

			ImGui::PopID();
		}
	}

	void ProjectLauncherPanel::DrawCreateSection() {
		if (m_NewLocation[0] == '\0') {
			std::filesystem::path docs;
			char* profile = nullptr;
			size_t len = 0;
			if (_dupenv_s(&profile, &len, "USERPROFILE") == 0 && profile) {
				docs = std::filesystem::path(profile) / "Documents" / "LunexProjects";
				free(profile);
			}
			strncpy_s(m_NewLocation, docs.string().c_str(), sizeof(m_NewLocation) - 1);
		}

		ImGui::SetCursorPos(ImVec2(32.0f, 24.0f));
		ImGui::BeginGroup();
		ImGui::SetWindowFontScale(1.3f);
		ImGui::TextUnformatted("New Project");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextColored(TextMuted(), "Creates the folder structure, a project file and a sample scene.");
		ImGui::Dummy(ImVec2(0, 16));

		// Template
		ImGui::TextColored(TextMuted(), "TEMPLATE");
		ImGui::Dummy(ImVec2(0, 4));
		{
			const ImVec2 tileSize(180.0f, 110.0f);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();
			dl->AddRectFilled(p, ImVec2(p.x + tileSize.x, p.y + tileSize.y), ImGui::GetColorU32(UI::Colors::BgLight().ToImVec4()), 4.0f);
			dl->AddRect(p, ImVec2(p.x + tileSize.x, p.y + tileSize.y), ImGui::GetColorU32(Accent()), 4.0f, 0, 2.0f);
			dl->AddText(ImVec2(p.x + 12, p.y + 12), ImGui::GetColorU32(ImGuiCol_Text), "Blank 3D");
			dl->AddText(ImVec2(p.x + 12, p.y + 34), ImGui::GetColorU32(TextMuted()), "Sun, sky, camera");
			ImGui::Dummy(tileSize);
			ImGui::SameLine(0, 12);
			ImVec2 q = ImGui::GetCursorScreenPos();
			dl->AddRectFilled(q, ImVec2(q.x + tileSize.x, q.y + tileSize.y), ImGui::GetColorU32(UI::Colors::BgMedium().ToImVec4()), 4.0f);
			dl->AddText(ImVec2(q.x + 12, q.y + 12), ImGui::GetColorU32(TextMuted()), "2D / Templates");
			dl->AddText(ImVec2(q.x + 12, q.y + 34), ImGui::GetColorU32(TextMuted()), "coming soon");
			ImGui::Dummy(tileSize);
		}
		ImGui::Dummy(ImVec2(0, 16));

		const float fieldW = 520.0f;
		ImGui::TextColored(TextMuted(), "PROJECT NAME");
		ImGui::SetNextItemWidth(fieldW);
		ImGui::InputText("##NewName", m_NewName, sizeof(m_NewName));
		ImGui::Dummy(ImVec2(0, 8));

		ImGui::TextColored(TextMuted(), "LOCATION");
		ImGui::SetNextItemWidth(fieldW - 90.0f);
		ImGui::InputText("##NewLocation", m_NewLocation, sizeof(m_NewLocation));
		ImGui::SameLine();
		if (ImGui::Button("Browse...", ImVec2(82.0f, 0))) {
			std::string folder = FileDialogs::SelectFolder();
			if (!folder.empty()) strncpy_s(m_NewLocation, folder.c_str(), sizeof(m_NewLocation) - 1);
		}

		const std::string name = m_NewName;
		const std::filesystem::path target = std::filesystem::path(m_NewLocation) / name;
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::TextColored(TextMuted(), "Will be created at: %s", (target / "project.lunex").string().c_str());

		// Validation
		std::string error;
		if (name.empty()) error = "Enter a project name.";
		else if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) error = "The name contains invalid characters.";
		else if (std::string(m_NewLocation).empty()) error = "Choose a location.";
		else if (std::filesystem::exists(target / "project.lunex")) error = "A project already exists there.";

		ImGui::Dummy(ImVec2(0, 16));
		ImGui::BeginDisabled(!error.empty());
		if (AccentButton("Create Project", ImVec2(180.0f, 36.0f)) && m_OnCreateProject) {
			std::error_code ec;
			std::filesystem::create_directories(m_NewLocation, ec);
			m_OnCreateProject(name, std::filesystem::path(m_NewLocation));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100.0f, 36.0f))) m_Section = Section::Projects;
		if (!error.empty()) ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s", error.c_str());
		ImGui::EndGroup();
	}

	void ProjectLauncherPanel::DrawVersionsSection() {
		ImGui::SetCursorPos(ImVec2(32.0f, 24.0f));
		ImGui::BeginGroup();
		ImGui::SetWindowFontScale(1.3f);
		ImGui::TextUnformatted("Versions");
		ImGui::SetWindowFontScale(1.0f);

		const bool hasProject = m_Selected >= 0 && m_Selected < static_cast<int>(m_Recent.size()) && m_Recent[m_Selected].Exists;
		if (!hasProject) {
			ImGui::TextColored(TextMuted(), "Select a project in 'Projects' to see and create its versions.");
			ImGui::EndGroup();
			return;
		}
		if (m_VersionsProject != m_Recent[m_Selected].ProjectFile) RefreshVersions();

		auto provider = ProjectVersioning::CreateProvider(m_VersionsProject);
		ImGui::TextColored(TextMuted(), "%s  -  %s", m_Recent[m_Selected].Name.c_str(), provider->GetName());
		ImGui::TextColored(TextMuted(), "Snapshots copy the project content into .lunex/versions. Restoring saves the current state first.");
		ImGui::Dummy(ImVec2(0, 12));

		ImGui::SetNextItemWidth(420.0f);
		ImGui::InputTextWithHint("##SnapshotMsg", "Describe this version (e.g. 'Level 1 lighting done')", m_SnapshotMessage, sizeof(m_SnapshotMessage));
		ImGui::SameLine();
		if (AccentButton("Create Snapshot", ImVec2(150.0f, 0))) {
			ProjectVersion v;
			if (provider->Commit(m_SnapshotMessage, false, &v)) {
				m_VersionStatus = "Created " + v.ID + " (" + std::to_string(v.FileCount) + " files)";
				m_SnapshotMessage[0] = '\0';
			}
			else m_VersionStatus = "Snapshot failed (see console)";
			RefreshVersions();
		}
		if (!m_VersionStatus.empty()) ImGui::TextColored(TextMuted(), "%s", m_VersionStatus.c_str());
		ImGui::Dummy(ImVec2(0, 12));

		if (m_Versions.empty()) {
			ImGui::TextColored(TextMuted(), "No versions yet.");
		}
		else if (ImGui::BeginTable("##Versions", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp,
			ImVec2(std::min(ImGui::GetContentRegionAvail().x - 32.0f, 980.0f), 0)))
		{
			ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("Message");
			ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 130.0f);
			ImGui::TableHeadersRow();

			std::string restoreID, deleteID;
			for (const auto& v : m_Versions) {
				ImGui::PushID(v.ID.c_str());
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextUnformatted(FormatTime(static_cast<std::time_t>(v.Timestamp)).c_str());
				ImGui::TableNextColumn();
				if (v.Automatic) ImGui::TextColored(TextMuted(), "%s (auto)", v.Message.c_str());
				else ImGui::TextUnformatted(v.Message.c_str());
				ImGui::TableNextColumn(); ImGui::TextUnformatted(v.Author.c_str());
				ImGui::TableNextColumn(); ImGui::Text("%.1f MB, %u files", v.SizeBytes / (1024.0 * 1024.0), v.FileCount);
				ImGui::TableNextColumn();
				if (ImGui::SmallButton("Restore")) ImGui::OpenPopup("##ConfirmRestore");
				ImGui::SameLine();
				if (ImGui::SmallButton("Delete")) deleteID = v.ID;
				if (ImGui::BeginPopup("##ConfirmRestore")) {
					ImGui::Text("Restore '%s'? The current state is saved as a snapshot first.", v.Message.c_str());
					if (AccentButton("Restore", ImVec2(120, 0))) { restoreID = v.ID; ImGui::CloseCurrentPopup(); }
					ImGui::SameLine();
					if (ImGui::Button("Cancel", ImVec2(90, 0))) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
			ImGui::EndTable();

			if (!restoreID.empty()) {
				m_VersionStatus = provider->Restore(restoreID) ? "Restored " + restoreID : "Restore failed";
				RefreshVersions();
			}
			if (!deleteID.empty()) {
				provider->Delete(deleteID);
				RefreshVersions();
			}
		}
		ImGui::EndGroup();
	}

} // namespace Lunex
