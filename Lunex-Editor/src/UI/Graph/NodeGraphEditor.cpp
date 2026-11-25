#include "stpch.h"
#include "NodeGraphEditor.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <glm/gtc/type_ptr.hpp>

namespace ed = ax::NodeEditor;

namespace Lunex::UI {

	using namespace Graph;

	namespace {
		ImVec4 PinColor(PinType type) {
			switch (type) {
				case PinType::Float:   return ImVec4(0.63f, 0.63f, 0.63f, 1.0f);   // grey
				case PinType::Vec2:    return ImVec4(0.39f, 0.78f, 0.39f, 1.0f);
				case PinType::Vec3:    return ImVec4(0.39f, 0.39f, 0.78f, 1.0f);
				case PinType::Color:   return ImVec4(0.85f, 0.78f, 0.20f, 1.0f);   // yellow
				case PinType::Texture: return ImVec4(0.85f, 0.51f, 0.17f, 1.0f);   // engine orange
				case PinType::Flow:    return ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
				default:               return ImVec4(0.70f, 0.40f, 0.85f, 1.0f);
			}
		}

		void DrawPinIcon(bool connected, const ImVec4& color) {
			const float size = ImGui::GetTextLineHeight();
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 c(p.x + size * 0.5f, p.y + size * 0.5f);
			ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
			if (connected) dl->AddCircleFilled(c, size * 0.32f, col);
			else dl->AddCircle(c, size * 0.32f, col, 0, 1.8f);
			ImGui::Dummy(ImVec2(size, size));
		}
	}

	NodeGraphEditor::NodeGraphEditor(const std::string& id) : m_ID(id) {
		ed::Config config;
		config.SettingsName = nullptr;   // layout lives in the asset (node positions)
		m_Context = ed::CreateEditor(&config);

		ed::SetCurrentEditor(m_Context);
		auto& style = ed::GetStyle();
		style.NodeRounding = 4.0f;
		style.NodeBorderWidth = 1.0f;
		style.HoveredNodeBorderWidth = 2.0f;
		style.SelectedNodeBorderWidth = 2.0f;
		style.Colors[ed::StyleColor_Bg] = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
		style.Colors[ed::StyleColor_Grid] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
		style.Colors[ed::StyleColor_NodeBg] = ImVec4(0.16f, 0.16f, 0.17f, 0.96f);
		style.Colors[ed::StyleColor_NodeBorder] = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
		style.Colors[ed::StyleColor_HovNodeBorder] = ImVec4(0.85f, 0.51f, 0.17f, 0.8f);
		style.Colors[ed::StyleColor_SelNodeBorder] = ImVec4(0.85f, 0.51f, 0.17f, 1.0f);
		style.Colors[ed::StyleColor_SelLinkBorder] = ImVec4(0.85f, 0.51f, 0.17f, 1.0f);
		ed::SetCurrentEditor(nullptr);
	}

	NodeGraphEditor::~NodeGraphEditor() {
		if (m_Context) ed::DestroyEditor(m_Context);
	}

	bool NodeGraphEditor::Draw(NodeGraph& graph, const char* emptyHint) {
		bool changed = false;
		if (&graph != m_LastGraph) {
			m_LastGraph = &graph;
			m_PositionsDirty = true;
			m_FrameDelay = 2;
		}

		ed::SetCurrentEditor(m_Context);
		ed::Begin(m_ID.c_str());

		for (auto& node : graph.GetNodes()) {
			if (m_PositionsDirty) ed::SetNodePosition(ed::NodeId(node.NodeID), ImVec2(node.Position.x, node.Position.y));
			DrawNode(graph, node, changed);
		}
		m_PositionsDirty = false;

		for (const auto& link : graph.GetLinks()) {
			const Pin* from = graph.FindPin(link.FromPin);
			ed::Link(ed::LinkId(link.LinkID), ed::PinId(link.FromPin), ed::PinId(link.ToPin),
				from ? PinColor(from->Type) : ImVec4(1, 1, 1, 1), 2.0f);
		}

		HandleCreation(graph, changed);
		HandleDeletion(graph, changed);
		DrawContextMenu(graph, changed);

		// Keep node positions in the data model (saved with the asset)
		for (auto& node : graph.GetNodes()) {
			ImVec2 p = ed::GetNodePosition(ed::NodeId(node.NodeID));
			if (p.x != FLT_MAX) node.Position = { p.x, p.y };
		}

		if (m_FrameDelay > 0 && !graph.GetNodes().empty() && --m_FrameDelay == 0)
			ed::NavigateToContent(0.0f);
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F, false) && !ImGui::GetIO().WantTextInput)
			ed::NavigateToContent();

		ed::End();
		ed::SetCurrentEditor(nullptr);

		if (graph.GetNodes().empty() && emptyHint) {
			ImVec2 min = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 12, min.y + 12), ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyHint);
		}
		if (changed) graph.Touch();
		return changed;
	}

	bool NodeGraphEditor::DrawInputDefault(Pin& pin) {
		if (pin.Type != PinType::Float && pin.Type != PinType::Color) return false;   // no inline editor
		ImGui::PushID(static_cast<int>(pin.PinID));
		bool edited = false;
		ImGui::SameLine();
		switch (pin.Type) {
			case PinType::Float:
				ImGui::SetNextItemWidth(70.0f);
				edited = ImGui::DragFloat("##v", &pin.DefaultValue.x, 0.01f, 0.0f, 0.0f, "%.2f");
				break;
			case PinType::Color:
				ImGui::SetNextItemWidth(150.0f);
				edited = ImGui::ColorEdit3("##c", glm::value_ptr(pin.DefaultValue),
					ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoLabel);
				break;
			default:
				break;
		}
		ImGui::PopID();
		return edited;
	}

	void NodeGraphEditor::DrawNode(NodeGraph& graph, Node& node, bool& changed) {
		ed::BeginNode(ed::NodeId(node.NodeID));
		ImGui::PushID(static_cast<int>(node.NodeID));

		// Title
		ImGui::TextColored(ImVec4(0.92f, 0.62f, 0.30f, 1.0f), "%s", node.Title.c_str());
		ImGui::Dummy(ImVec2(0, 2));

		// Inputs
		for (auto& pin : node.Inputs) {
			const bool connected = graph.GetLinkInto(pin.PinID) != nullptr;
			ed::BeginPin(ed::PinId(pin.PinID), ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
			DrawPinIcon(connected, PinColor(pin.Type));
			ImGui::SameLine();
			ImGui::TextUnformatted(pin.Name.c_str());
			ed::EndPin();
			if (!connected && DrawInputDefault(pin)) changed = true;
		}

		// Custom body (constants, texture pickers...)
		if (DrawNodeBody && DrawNodeBody(node)) changed = true;

		// Outputs (right aligned)
		float maxWidth = 0.0f;
		for (const auto& pin : node.Outputs) maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.Name.c_str()).x);
		for (auto& pin : node.Outputs) {
			bool connected = false;
			for (const auto& l : graph.GetLinks()) if (l.FromPin == pin.PinID) { connected = true; break; }
			float indent = 80.0f + maxWidth - ImGui::CalcTextSize(pin.Name.c_str()).x;
			ImGui::Dummy(ImVec2(indent, 0));
			ImGui::SameLine();
			ed::BeginPin(ed::PinId(pin.PinID), ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
			ImGui::TextUnformatted(pin.Name.c_str());
			ImGui::SameLine();
			DrawPinIcon(connected, PinColor(pin.Type));
			ed::EndPin();
		}

		ImGui::PopID();
		ed::EndNode();
	}

	void NodeGraphEditor::HandleCreation(NodeGraph& graph, bool& changed) {
		if (ed::BeginCreate(ImVec4(0.85f, 0.51f, 0.17f, 1.0f), 2.0f)) {
			ed::PinId a, b;
			if (ed::QueryNewLink(&a, &b) && a && b) {
				if (graph.CanConnect(a.Get(), b.Get())) {
					if (ed::AcceptNewItem(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), 3.0f)) {
						if (graph.Connect(a.Get(), b.Get())) changed = true;
					}
				}
				else {
					ed::RejectNewItem(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), 2.0f);
				}
			}
			ed::EndCreate();   // only after a successful BeginCreate in this node-editor version
		}
	}

	void NodeGraphEditor::HandleDeletion(NodeGraph& graph, bool& changed) {
		if (ed::BeginDelete()) {
			ed::LinkId linkId;
			while (ed::QueryDeletedLink(&linkId)) {
				if (ed::AcceptDeletedItem() && graph.RemoveLink(linkId.Get())) changed = true;
			}
			ed::NodeId nodeId;
			while (ed::QueryDeletedNode(&nodeId)) {
				const Node* node = graph.FindNode(nodeId.Get());
				const NodeTypeInfo* info = (node && graph.GetRegistry()) ? graph.GetRegistry()->Get(node->TypeName) : nullptr;
				if (info && info->Unique) { ed::RejectDeletedItem(); continue; }
				if (ed::AcceptDeletedItem() && graph.RemoveNode(nodeId.Get())) changed = true;
			}
			ed::EndDelete();
		}
	}

	void NodeGraphEditor::DrawContextMenu(NodeGraph& graph, bool& changed) {
		ed::Suspend();
		static ImVec2 s_SpawnPos;
		if (ed::ShowBackgroundContextMenu()) {
			s_SpawnPos = ImGui::GetMousePos();
			ImGui::OpenPopup("##AddNode");
		}
		if (ImGui::BeginPopup("##AddNode")) {
			ImGui::TextDisabled("Add Node");
			ImGui::Separator();
			if (const NodeRegistry* registry = graph.GetRegistry()) {
				std::string lastCategory;
				for (const auto& typeName : registry->GetOrder()) {
					const NodeTypeInfo* info = registry->Get(typeName);
					if (!info || (info->Unique && graph.FindNodeByType(typeName))) continue;
					if (info->Category != lastCategory) {
						if (!lastCategory.empty()) ImGui::Separator();
						ImGui::TextDisabled("%s", info->Category.c_str());
						lastCategory = info->Category;
					}
					if (ImGui::MenuItem(info->Title.c_str())) {
						ed::Resume();
						ImVec2 canvas = ed::ScreenToCanvas(s_SpawnPos);
						ed::Suspend();
						if (Node* node = graph.AddNode(typeName, { canvas.x, canvas.y })) {
							ed::SetNodePosition(ed::NodeId(node->NodeID), canvas);
							changed = true;
						}
					}
				}
			}
			ImGui::EndPopup();
		}
		ed::Resume();
	}

} // namespace Lunex::UI
