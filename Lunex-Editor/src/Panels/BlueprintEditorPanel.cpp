#include "stpch.h"
#include "BlueprintEditorPanel.h"

#include "Project/Project.h"
#include "Log/Log.h"

#include "../UI/UICore.h"
#include "../UI/Lunexi/Component.h"

#include <imgui.h>

namespace Lunex {

	using namespace Graph;

	BlueprintEditorPanel::BlueprintEditorPanel() {
		m_Editor = std::make_unique<UI::NodeGraphEditor>("BlueprintEditor");
		s_Instance = this;
	}

	BlueprintEditorPanel::~BlueprintEditorPanel() {
		if (s_Instance == this) s_Instance = nullptr;
	}

	std::filesystem::path BlueprintEditorPanel::CreateForEntity(const std::string& entityName) {
		std::filesystem::path dir = Project::GetActive() ? Project::GetActiveAssetDirectory() / "Blueprints" : std::filesystem::path("Blueprints");
		std::string base = entityName.empty() ? "Blueprint" : entityName;
		for (char& c : base) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
		std::filesystem::path path = dir / (base + ".lublueprint");
		for (int i = 1; std::filesystem::exists(path); i++) path = dir / (base + "_" + std::to_string(i) + ".lublueprint");
		Blueprint::BlueprintAsset::CreateNew(path);
		LNX_LOG_INFO("Blueprint created: {0}", path.string());
		return path;
	}

	void BlueprintEditorPanel::Open(const std::filesystem::path& path) {
		if (m_Asset && m_Dirty) Save();
		m_Asset = Blueprint::BlueprintAsset::Load(path);
		if (!m_Asset) return;
		m_Open = true;
		m_Dirty = false;
		m_Focus = 3;
		m_Editor->RequestFrameAll();
	}

	void BlueprintEditorPanel::Save() {
		if (!m_Asset) return;
		if (m_Asset->Save()) m_Dirty = false;
		else LNX_LOG_ERROR("Failed to save blueprint {0}", m_Asset->Path.string());
	}

	void BlueprintEditorPanel::OnImGuiRender() {
		if (!m_Open || !m_Asset || !m_Asset->Graph) return;

		const std::string title = "Blueprint - " + m_Asset->Name + (m_Dirty ? " *" : "") + "###BlueprintEditor";
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.8f, vp->WorkSize.y * 0.75f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.1f, vp->WorkPos.y + vp->WorkSize.y * 0.12f), ImGuiCond_FirstUseEver);
		if (m_Focus > 0) { ImGui::SetNextWindowFocus(); m_Focus--; }

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		bool open = true;
		const bool visible = ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
		ImGui::PopStyleVar();
		if (visible) {
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
				Save();

			// Toolbar
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::Colors::BgDark().ToImVec4());
			ImGui::BeginChild("##bptoolbar", ImVec2(0, ImGui::GetFrameHeight() + 10.0f), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
			if (ImGui::Button("Save")) Save();
			ImGui::SameLine();
			if (ImGui::Button("Frame All")) m_Editor->RequestFrameAll();
			ImGui::SameLine();
			ImGui::TextDisabled("|  %s", m_Asset->Path.filename().string().c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("  Runs in Play mode on the entities whose Script component uses it");
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();

			const float footer = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
			ImGui::BeginChild("##bpbody", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			UI::Layouts::SplitProps split;
			split.ID = "BlueprintSplit";
			split.Ratio = &m_SidebarRatio;
			split.MinLeft = 180.0f;
			split.MinRight = 300.0f;
			UI::Slots slots;
			slots.Named["Left"] = [this] { DrawSidebar(*m_Asset->Graph); };
			slots.Named["Right"] = [this] {
				if (m_Editor->Draw(*m_Asset->Graph, "Shift+A or right-click to add nodes. Start from On Start / On Update.")) m_Dirty = true;
			};
			UI::Layouts::Split(split, slots);
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(10.0f, ImGui::GetCursorPosY() + 3.0f));
			ImGui::TextDisabled("White links = execution order   |   Shift+A Add   |   Drag a connected input: detach   |   X Delete   |   Shift+D Duplicate   |   F Frame   |   Ctrl+S Save");
		}
		ImGui::End();
		if (!open) {
			if (m_Dirty) Save();
			m_Open = false;
		}
	}

	void BlueprintEditorPanel::DrawSidebar(NodeGraph& graph) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		ImGui::BeginChild("##bpside", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

		ImGui::SeparatorText("Selected");
		if (const Node* node = graph.FindNode(m_Editor->GetSelectedNode())) {
			ImGui::TextUnformatted(node->Title.c_str());
			const NodeTypeInfo* info = graph.GetRegistry() ? graph.GetRegistry()->Get(node->TypeName) : nullptr;
			if (info) {
				ImGui::TextDisabled("%s", info->Category.c_str());
				if (!info->Description.empty()) ImGui::TextWrapped("%s", info->Description.c_str());
			}
		}
		else {
			ImGui::TextDisabled("No node selected");
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Events");
		int events = 0;
		for (const auto& n : graph.GetNodes()) {
			if (n.TypeName.rfind("BP.On", 0) != 0) continue;
			ImGui::BulletText("%s", n.Title.c_str());
			events++;
		}
		if (!events) ImGui::TextDisabled("Add an event (Events menu)");

		ImGui::Spacing();
		ImGui::SeparatorText("Variables");
		std::vector<std::string> names;
		for (const auto& n : graph.GetNodes()) {
			if (n.TypeName != "BP.GetVariable" && n.TypeName != "BP.SetVariable" && n.TypeName != "BP.SetVectorVariable") continue;
			auto it = n.Properties.find("Name");
			if (it != n.Properties.end() && !it->second.empty() && std::find(names.begin(), names.end(), it->second) == names.end())
				names.push_back(it->second);
		}
		for (const auto& v : names) ImGui::BulletText("%s", v.c_str());
		if (names.empty()) ImGui::TextDisabled("Get / Set Variable nodes create them");

		ImGui::Spacing();
		ImGui::SeparatorText("Tips");
		ImGui::TextWrapped("Target pins default to the entity running the blueprint. Use Find Entity or Self to act on others.");
		ImGui::TextWrapped("Materials: name a Value node in a node-graph material and change it with Set Material Parameter.");

		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

} // namespace Lunex
