#include "stpch.h"
#include "NodeGraphEditor.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace Lunex::UI {

	using namespace Graph;

	// ============================================================================
	// COLORS (Blender palette)
	// ============================================================================

	ImVec4 PinTypeColor(PinType type) {
		switch (type) {
			case PinType::Float:   return ImVec4(0.63f, 0.63f, 0.63f, 1.0f);
			case PinType::Vec2:
			case PinType::Vec3:    return ImVec4(0.39f, 0.39f, 0.78f, 1.0f);
			case PinType::Color:
			case PinType::Texture: return ImVec4(0.78f, 0.78f, 0.16f, 1.0f);
			case PinType::Shader:  return ImVec4(0.39f, 0.78f, 0.39f, 1.0f);
			case PinType::Flow:    return ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
			case PinType::Bool:    return ImVec4(0.80f, 0.65f, 0.84f, 1.0f);
			case PinType::Int:     return ImVec4(0.35f, 0.55f, 0.36f, 1.0f);
			case PinType::String:  return ImVec4(0.44f, 0.70f, 1.00f, 1.0f);
			case PinType::Entity:  return ImVec4(0.00f, 0.84f, 0.64f, 1.0f);
			default:               return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
		}
	}

	namespace {
		ImVec4 CategoryColor(const std::string& category) {
			static const std::map<std::string, ImVec4> colors = {
				{ "Input",     ImVec4(0.514f, 0.192f, 0.290f, 1.0f) },
				{ "Output",    ImVec4(0.388f, 0.075f, 0.153f, 1.0f) },
				{ "Shader",    ImVec4(0.169f, 0.396f, 0.169f, 1.0f) },
				{ "Texture",   ImVec4(0.475f, 0.275f, 0.114f, 1.0f) },
				{ "Color",     ImVec4(0.427f, 0.412f, 0.153f, 1.0f) },
				{ "Vector",    ImVec4(0.235f, 0.235f, 0.514f, 1.0f) },
				{ "Converter", ImVec4(0.141f, 0.384f, 0.514f, 1.0f) },
				{ "Utilities", ImVec4(0.141f, 0.384f, 0.514f, 1.0f) },
				{ "Displacement", ImVec4(0.235f, 0.235f, 0.514f, 1.0f) },
				{ "Group",     ImVec4(0.161f, 0.380f, 0.322f, 1.0f) },
				{ "Layout",    ImVec4(0.30f, 0.30f, 0.32f, 1.0f) },
				// Blueprints
				{ "Events",    ImVec4(0.55f, 0.12f, 0.12f, 1.0f) },
				{ "Flow",      ImVec4(0.30f, 0.30f, 0.32f, 1.0f) },
				{ "Transform", ImVec4(0.16f, 0.36f, 0.55f, 1.0f) },
				{ "Physics",   ImVec4(0.46f, 0.30f, 0.12f, 1.0f) },
				{ "Material",  ImVec4(0.18f, 0.42f, 0.24f, 1.0f) },
				{ "Particles", ImVec4(0.52f, 0.22f, 0.50f, 1.0f) },
				{ "Input Devices", ImVec4(0.514f, 0.192f, 0.290f, 1.0f) },
				{ "Math",      ImVec4(0.141f, 0.384f, 0.514f, 1.0f) },
				{ "Debug",     ImVec4(0.35f, 0.35f, 0.38f, 1.0f) },
			};
			auto it = colors.find(category);
			return it != colors.end() ? it->second : ImVec4(0.32f, 0.32f, 0.34f, 1.0f);
		}

		ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

		std::string Lower(std::string s) {
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		const NodeTypeInfo* InfoOf(const NodeGraph& graph, const Node& node) {
			return graph.GetTypeInfo(node.TypeName);
		}

		bool IsPinVisible(const NodeTypeInfo* info, const Node& node, const Pin& pin) {
			return !(info && info->PinVisible) || info->PinVisible(node, pin);
		}

		constexpr float kSocketRadius = 5.0f;
		constexpr float kMargin = 10.0f;
	}

	// ============================================================================
	// LIFETIME
	// ============================================================================

	NodeGraphEditor::NodeGraphEditor(const std::string& id) : m_ID(id) {
		ed::Config config;
		config.SettingsName = nullptr;          // layout lives in the asset (node positions)
		config.NavigateButtonIndex = 2;         // middle mouse pans (Blender)
		config.ContextMenuButtonIndex = 1;      // right click: menus
		m_Context = ed::CreateEditor(&config);

		ed::SetCurrentEditor(m_Context);
		auto& style = ed::GetStyle();
		style.NodePadding = ImVec4(0, 0, 0, 0);
		style.NodeRounding = 5.0f;
		style.NodeBorderWidth = 1.0f;
		style.HoveredNodeBorderWidth = 1.5f;
		style.SelectedNodeBorderWidth = 2.0f;
		style.PinRounding = 0.0f;
		style.PinBorderWidth = 0.0f;
		style.LinkStrength = 80.0f;
		style.Colors[ed::StyleColor_Bg] = ImVec4(0.114f, 0.114f, 0.114f, 1.0f);
		style.Colors[ed::StyleColor_Grid] = ImVec4(1.0f, 1.0f, 1.0f, 0.045f);
		style.Colors[ed::StyleColor_NodeBg] = ImVec4(0.188f, 0.188f, 0.188f, 0.97f);
		style.Colors[ed::StyleColor_NodeBorder] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
		style.Colors[ed::StyleColor_HovNodeBorder] = ImVec4(0.85f, 0.85f, 0.85f, 0.6f);
		style.Colors[ed::StyleColor_SelNodeBorder] = ImVec4(1.0f, 0.63f, 0.20f, 1.0f);
		style.Colors[ed::StyleColor_SelLinkBorder] = ImVec4(1.0f, 0.63f, 0.20f, 1.0f);
		style.Colors[ed::StyleColor_PinRect] = ImVec4(1.0f, 1.0f, 1.0f, 0.15f);
		style.Colors[ed::StyleColor_PinRectBorder] = ImVec4(0, 0, 0, 0);
		// The selection rectangle is drawn by us above the nodes (the built-in one sits under them)
		style.Colors[ed::StyleColor_NodeSelRect] = ImVec4(0, 0, 0, 0);
		style.Colors[ed::StyleColor_NodeSelRectBorder] = ImVec4(0, 0, 0, 0);
		style.Colors[ed::StyleColor_LinkSelRect] = ImVec4(0, 0, 0, 0);
		style.Colors[ed::StyleColor_LinkSelRectBorder] = ImVec4(0, 0, 0, 0);
		ed::SetCurrentEditor(nullptr);
	}

	NodeGraphEditor::~NodeGraphEditor() {
		if (m_Context) ed::DestroyEditor(m_Context);
	}

	void NodeGraphEditor::RequestPopup(std::function<bool(NodeGraph&)> content) {
		m_PopupContent = std::move(content);
		m_OpenPopup = true;
	}

	// ============================================================================
	// DRAW
	// ============================================================================

	NodeGraph& NodeGraphEditor::ResolveCurrentGraph(NodeGraph& root) {
		NodeGraph* current = &root;
		for (size_t i = 0; i < m_GroupPath.size(); i++) {
			Node* group = current->FindNode(m_GroupPath[i]);
			if (!group || !group->Subgraph) { m_GroupPath.resize(i); break; }
			current = group->Subgraph.get();
		}
		return *current;
	}

	bool NodeGraphEditor::Draw(NodeGraph& root, const char* emptyHint) {
		bool changed = false;
		NodeGraph& graph = ResolveCurrentGraph(root);
		m_CurrentGraph = &graph;
		if (&graph != m_LastGraph) {
			m_LastGraph = &graph;
			m_PositionsDirty = true;
			m_FrameDelay = 2;
			m_SocketPos.clear();
			m_InsertLink = 0;
		}
		m_ScreenStroke.clear();
		m_ScreenInsert.clear();
		m_ScreenLazy.clear();

		ed::SetCurrentEditor(m_Context);
		ed::Begin(m_ID.c_str());

		// Nodes (index loop: the body callbacks never add nodes, but be safe with references)
		for (size_t i = 0; i < graph.GetNodes().size(); i++) {
			Node& node = graph.GetNodes()[i];
			if (m_PositionsDirty) ed::SetNodePosition(ed::NodeId(node.NodeID), ImVec2(node.Position.x, node.Position.y));
			DrawNode(graph, node, changed);
		}
		m_PositionsDirty = false;

		// Links tinted by the source socket type
		for (const auto& link : graph.GetLinks()) {
			const Pin* from = graph.FindPin(link.FromPin);
			ImVec4 color = from ? PinTypeColor(from->Type) : ImVec4(1, 1, 1, 1);
			ed::Link(ed::LinkId(link.LinkID), ed::PinId(link.FromPin), ed::PinId(link.ToPin), color, from && from->Type == PinType::Flow ? 3.0f : 2.2f);
		}

		HandleCreation(graph, changed);
		HandleDeletion(graph, changed);
		HandleCanvasTools(graph, changed);
		HandleShortcuts(graph, changed);

		// Keep node positions in the data model (saved with the asset)
		for (auto& node : graph.GetNodes()) {
			ImVec2 p = ed::GetNodePosition(ed::NodeId(node.NodeID));
			if (p.x != FLT_MAX && (p.x != node.Position.x || p.y != node.Position.y)) node.Position = { p.x, p.y };
		}

		// Selection
		m_SelectedNode = 0;
		if (ed::GetSelectedObjectCount() > 0) {
			ed::NodeId selected[1];
			if (ed::GetSelectedNodes(selected, 1) > 0) m_SelectedNode = selected[0].Get();
		}

		if (m_FrameDelay > 0 && !graph.GetNodes().empty() && --m_FrameDelay == 0)
			ed::NavigateToContent(0.0f);

		DrawPopups(graph, changed);

		ed::End();
		ed::SetCurrentEditor(nullptr);

		DrawOverlays(root);

		if (graph.GetNodes().empty() && emptyHint) {
			ImVec2 min = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 12, min.y + 12), ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyHint);
		}
		if (changed) {
			graph.Touch();
			if (&graph != &root) {
				// Edits inside a group: refresh the group sockets and recompile the material
				root.SyncGroupsDeep();
				root.Touch();
			}
		}
		return changed;
	}

	void NodeGraphEditor::DrawOverlays(NodeGraph& root) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 canvasMin = ImGui::GetItemRectMin();

		// Box selection above the nodes (imgui-node-editor draws its own one under them)
		if (m_BoxSelecting) {
			const ImVec2 a(std::min(m_ScreenBoxA.x, m_ScreenBoxB.x), std::min(m_ScreenBoxA.y, m_ScreenBoxB.y));
			const ImVec2 b(std::max(m_ScreenBoxA.x, m_ScreenBoxB.x), std::max(m_ScreenBoxA.y, m_ScreenBoxB.y));
			dl->AddRectFilled(a, b, IM_COL32(255, 160, 50, 28));
			dl->AddRect(a, b, IM_COL32(255, 160, 50, 200), 0.0f, 0, 1.0f);
		}
		// Knife (cut links) / reroute strokes
		if (m_ScreenStroke.size() >= 2) {
			const ImU32 col = m_Stroke == Stroke::Knife ? IM_COL32(255, 80, 80, 230) : IM_COL32(120, 200, 255, 230);
			for (size_t i = 1; i < m_ScreenStroke.size(); i++) {
				// Dashed like Blender's knife
				if ((i / 2) % 2 == 0) dl->AddLine(m_ScreenStroke[i - 1], m_ScreenStroke[i], col, 2.0f);
			}
		}
		// Link that will receive the dragged node
		if (m_ScreenInsert.size() >= 2) dl->AddPolyline(m_ScreenInsert.data(), static_cast<int>(m_ScreenInsert.size()), IM_COL32(255, 255, 255, 220), 0, 4.0f);
		// Lazy connect
		if (m_ScreenLazy.size() == 2) {
			dl->AddLine(m_ScreenLazy[0], m_ScreenLazy[1], IM_COL32(255, 200, 80, 230), 2.0f);
			dl->AddCircleFilled(m_ScreenLazy[1], 4.0f, IM_COL32(255, 200, 80, 230));
		}
		// Color being dragged between swatches
		if (m_DraggingColor && ImGui::GetDragDropPayload()) {
			const ImVec2 m = ImGui::GetIO().MousePos;
			dl->AddRectFilled(ImVec2(m.x + 12, m.y + 12), ImVec2(m.x + 36, m.y + 30), ImGui::ColorConvertFloat4ToU32(ImVec4(m_DragColor.r, m_DragColor.g, m_DragColor.b, 1.0f)), 3.0f);
			dl->AddRect(ImVec2(m.x + 12, m.y + 12), ImVec2(m.x + 36, m.y + 30), IM_COL32(0, 0, 0, 255), 3.0f);
		}
		else {
			m_DraggingColor = false;
		}

		// Breadcrumb while editing a group (Blender shows the group path in the header)
		if (!m_GroupPath.empty()) {
			std::string path = "Material";
			NodeGraph* g = &root;
			for (ID id : m_GroupPath) {
				Node* n = g->FindNode(id);
				if (!n || !n->Subgraph) break;
				path += "  >  " + n->Title;
				g = n->Subgraph.get();
			}
			path += "      (Tab: exit group)";
			const ImVec2 size = ImGui::CalcTextSize(path.c_str());
			dl->AddRectFilled(ImVec2(canvasMin.x + 8, canvasMin.y + 8), ImVec2(canvasMin.x + 20 + size.x, canvasMin.y + 14 + size.y), IM_COL32(40, 70, 60, 230), 4.0f);
			dl->AddText(ImVec2(canvasMin.x + 14, canvasMin.y + 11), IM_COL32(230, 255, 240, 255), path.c_str());
		}
	}

	// ============================================================================
	// INLINE FIELDS
	// ============================================================================

	bool NodeGraphEditor::SliderField(const char* id, const char* label, float* value, float min, float max, float width, bool isInt) {
		static ImGuiID s_EditingID = 0;
		static int s_EditFrames = 0;
		const ImGuiID gid = ImGui::GetID(id);
		const float h = ImGui::GetFrameHeight();
		bool changed = false;

		// Double-click: type a value
		if (s_EditingID == gid) {
			ImGui::SetNextItemWidth(width);
			if (s_EditFrames++ == 0) ImGui::SetKeyboardFocusHere();
			changed = ImGui::InputFloat(id, value, 0.0f, 0.0f, isInt ? "%.0f" : "%.3f", ImGuiInputTextFlags_AutoSelectAll);
			if (s_EditFrames > 2 && !ImGui::IsItemActive()) s_EditingID = 0;
			return changed;
		}

		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(id, ImVec2(width, h));
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		const bool bounded = max > min;
		ImGuiIO& io = ImGui::GetIO();

		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { s_EditingID = gid; s_EditFrames = 0; }
		else if (active) {
			float v = *value;
			if (bounded) {
				// Factor sliders: the value follows the mouse (Blender)
				const float t = std::clamp((ImGui::GetMousePos().x - p.x) / std::max(width, 1.0f), 0.0f, 1.0f);
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) || ImGui::IsItemActivated()) v = min + t * (max - min);
			}
			else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
				const float speed = (io.KeyShift ? 0.001f : 0.01f) * std::max(1.0f, std::abs(*value) * 0.25f);
				v += io.MouseDelta.x * speed;
			}
			if (isInt) v = std::round(v);
			if (v != *value) { *value = v; changed = true; }
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 q(p.x + width, p.y + h);
		dl->AddRectFilled(p, q, U32(active ? ImVec4(0.27f, 0.27f, 0.27f, 1) : hovered ? ImVec4(0.40f, 0.40f, 0.40f, 1) : ImVec4(0.33f, 0.33f, 0.33f, 1)), 4.0f);
		if (bounded) {
			const float t = std::clamp((*value - min) / (max - min), 0.0f, 1.0f);
			if (t > 0.0f) {
				dl->PushClipRect(p, ImVec2(p.x + width * t, q.y), true);
				dl->AddRectFilled(p, q, U32(ImVec4(0.278f, 0.447f, 0.702f, 1.0f)), 4.0f);
				dl->PopClipRect();
			}
		}
		char buf[32];
		if (isInt) snprintf(buf, sizeof(buf), "%d", static_cast<int>(*value));
		else snprintf(buf, sizeof(buf), "%.3f", *value);
		const float textY = p.y + (h - ImGui::GetTextLineHeight()) * 0.5f;
		dl->PushClipRect(p, q, true);
		const float valueW = ImGui::CalcTextSize(buf).x;
		if (label && *label) {
			dl->AddText(ImVec2(p.x + 6, textY), U32(ImVec4(0.92f, 0.92f, 0.92f, 1)), label);
			dl->AddText(ImVec2(q.x - 6 - valueW, textY), U32(ImVec4(0.92f, 0.92f, 0.92f, 1)), buf);
		}
		else {
			dl->AddText(ImVec2(p.x + (width - valueW) * 0.5f, textY), U32(ImVec4(0.92f, 0.92f, 0.92f, 1)), buf);
		}
		dl->PopClipRect();
		return changed;
	}

	bool NodeGraphEditor::ColorField(ID nodeID, ID pinID, glm::vec4& color, float width, float height) {
		const float h = height > 0.0f ? height : ImGui::GetFrameHeight();
		const bool clicked = ImGui::ColorButton("##swatch", ImVec4(color.r, color.g, color.b, color.a),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(width, h));
		bool dropped = false;
		// Drag a swatch onto another swatch to copy the color (also accepts colors from other panels)
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
			ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, glm::value_ptr(color), sizeof(float) * 4);
			m_DraggingColor = true;
			m_DragColor = color;
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F)) {
				std::memcpy(glm::value_ptr(color), p->Data, sizeof(float) * 4);
				dropped = true;
			}
			else if (const ImGuiPayload* p3 = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
				std::memcpy(glm::value_ptr(color), p3->Data, sizeof(float) * 3);
				dropped = true;
			}
			ImGui::EndDragDropTarget();
		}
		if (dropped) return true;
		if (clicked) {
			RequestPopup([nodeID, pinID](NodeGraph& g) {
				Node* n = g.FindNode(nodeID);
				if (!n) return false;
				glm::vec4* target = &n->Value;
				if (pinID) { Pin* pin = g.FindPin(pinID); if (!pin) return false; target = &pin->DefaultValue; }
				ImGui::SetNextItemWidth(220.0f);
				return ImGui::ColorPicker4("##picker", glm::value_ptr(*target),
					ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
			});
		}
		return false;
	}

	// ============================================================================
	// NODE
	// ============================================================================

	void NodeGraphEditor::DrawSocket(const Pin& pin, bool connected, const ImVec2& c) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImU32 col = U32(PinTypeColor(pin.Type));
		const ImU32 outline = U32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
		if (pin.Name == NewSocketPin) {
			// Empty group socket: drag a link onto it to add a socket to the group
			dl->AddCircle(c, kSocketRadius, U32(ImVec4(0.75f, 0.75f, 0.75f, 0.9f)), 0, 1.5f);
			return;
		}
		if (pin.Type == PinType::Flow) {
			// Execution pins: arrow
			const float r = kSocketRadius + 1.0f;
			ImVec2 a(c.x - r, c.y - r), b(c.x + r, c.y), d(c.x - r, c.y + r);
			if (connected) dl->AddTriangleFilled(a, b, d, col);
			else dl->AddTriangle(a, b, d, col, 1.5f);
			return;
		}
		if (pin.Type == PinType::Shader) {
			dl->AddCircleFilled(c, kSocketRadius + 1.0f, outline);
			dl->AddCircleFilled(c, kSocketRadius, col);
			if (!connected) dl->AddCircleFilled(c, kSocketRadius * 0.4f, outline);
			return;
		}
		if (pin.Type == PinType::Vec3 || pin.Type == PinType::Vec2) {
			// Vectors: diamond-ish circle with dot like Blender's "point" socket
			dl->AddCircleFilled(c, kSocketRadius + 1.0f, outline);
			dl->AddCircleFilled(c, kSocketRadius, col);
			if (!connected) dl->AddCircleFilled(c, 1.6f, outline);
			return;
		}
		dl->AddCircleFilled(c, kSocketRadius + 1.0f, outline);
		dl->AddCircleFilled(c, kSocketRadius, col);
	}

	bool NodeGraphEditor::DrawInputValue(Node& node, Pin& pin, float width) {
		bool changed = false;
		const char* label = pin.DisplayName().c_str();
		switch (pin.Type) {
			case PinType::Float:
				changed = SliderField("##v", label, &pin.DefaultValue.x, pin.Range.x, pin.Range.y, width);
				break;
			case PinType::Int:
				changed = SliderField("##i", label, &pin.DefaultValue.x, pin.Range.x, pin.Range.y, width, true);
				break;
			case PinType::Bool: {
				bool v = pin.DefaultValue.x != 0.0f;
				if (ImGui::Checkbox(label, &v)) { pin.DefaultValue.x = v ? 1.0f : 0.0f; changed = true; }
				break;
			}
			case PinType::Color:
			case PinType::Texture: {
				const float swatch = std::min(70.0f, width * 0.45f);
				const ImVec2 p = ImGui::GetCursorScreenPos();
				ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f));
				ImGui::TextUnformatted(label);
				ImGui::SetCursorScreenPos(ImVec2(p.x + width - swatch, p.y));
				if (ColorField(node.NodeID, pin.PinID, pin.DefaultValue, swatch)) changed = true;
				break;
			}
			case PinType::Vec2:
			case PinType::Vec3: {
				ImGui::TextUnformatted(label);
				const int n = pin.Type == PinType::Vec2 ? 2 : 3;
				static const char* axes[] = { "X", "Y", "Z" };
				for (int i = 0; i < n; i++) {
					ImGui::PushID(i);
					changed |= SliderField("##c", axes[i], &pin.DefaultValue[i], pin.Range.x, pin.Range.y, width);
					ImGui::PopID();
				}
				break;
			}
			case PinType::String: {
				char buf[128];
				strncpy_s(buf, pin.StringValue.c_str(), sizeof(buf) - 1);
				ImGui::SetNextItemWidth(width);
				if (ImGui::InputTextWithHint("##s", label, buf, sizeof(buf))) { pin.StringValue = buf; changed = true; }
				break;
			}
			default:
				ImGui::TextUnformatted(label);
				break;
		}
		return changed;
	}

	void NodeGraphEditor::DrawNode(NodeGraph& graph, Node& node, bool& changed) {
		const NodeTypeInfo* info = InfoOf(graph, node);
		const float W = (info && info->Width > 0.0f) ? info->Width : 160.0f;
		const float inner = W - 2.0f * kMargin;
		const float rowH = ImGui::GetFrameHeight() + 2.0f;

		// Which of this node's pins are linked
		std::unordered_set<ID> linked;
		for (const auto& l : graph.GetLinks()) { linked.insert(l.FromPin); linked.insert(l.ToPin); }

		if (node.IsReroute()) {
			DrawReroute(graph, node, !node.Inputs.empty() && linked.count(node.Inputs[0].PinID) > 0);
			return;
		}

		ed::BeginNode(ed::NodeId(node.NodeID));
		ImGui::PushID(static_cast<int>(node.NodeID));
		const ImVec2 origin = ImGui::GetCursorScreenPos();

		// ---- Header ----
		ImGui::Dummy(ImVec2(W, 2.0f));
		ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, origin.y + 4.0f));
		ImGui::TextUnformatted(node.Title.c_str());
		const float headerBottom = ImGui::GetItemRectMax().y + 4.0f;
		ImGui::SetCursorScreenPos(ImVec2(origin.x, headerBottom + 4.0f));

		auto socketRow = [&](Pin& pin, bool output, float y) {
			const ImVec2 c(output ? origin.x + W : origin.x, y + rowH * 0.5f - 1.0f);
			m_SocketPos[pin.PinID] = c;
			const ImVec2 back = ImGui::GetCursorScreenPos();
			ed::BeginPin(ed::PinId(pin.PinID), output ? ed::PinKind::Output : ed::PinKind::Input);
			ed::PinPivotRect(c, c);
			ed::PinRect(ImVec2(c.x - 8.0f, c.y - 8.0f), ImVec2(c.x + 8.0f, c.y + 8.0f));
			ed::EndPin();
			ImGui::SetCursorScreenPos(back);
			DrawSocket(pin, linked.count(pin.PinID) > 0, c);
		};

		// ---- Outputs (top, right aligned) ----
		const bool hideUnused = node.Properties.count("__hide") && node.Properties.at("__hide") == "1";
		for (auto& pin : node.Outputs) {
			if (!IsPinVisible(info, node, pin)) continue;
			if (hideUnused && !linked.count(pin.PinID)) continue;
			const float y = ImGui::GetCursorScreenPos().y;
			const char* name = pin.DisplayName().c_str();
			const float tw = ImGui::CalcTextSize(name).x;
			ImGui::SetCursorScreenPos(ImVec2(origin.x + W - kMargin - tw, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f));
			ImGui::TextUnformatted(name);
			ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
			socketRow(pin, true, y);
			ImGui::SetCursorScreenPos(ImVec2(origin.x, y + rowH));
		}

		// ---- Drop-downs / toggles ----
		if (info) {
			for (const auto& e : info->Enums) {
				const float y = ImGui::GetCursorScreenPos().y + 2.0f;
				ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, y));
				auto found = node.Properties.find(e.Key);
				const std::string current = (found != node.Properties.end() && !found->second.empty()) ? found->second : (e.Options.empty() ? "" : e.Options[0]);
				ImGui::PushID(e.Key.c_str());
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const float h = ImGui::GetFrameHeight();
				if (ImGui::InvisibleButton("##enum", ImVec2(inner, h))) {
					const ID nodeID = node.NodeID;
					const std::string key = e.Key;
					const std::vector<std::string> options = e.Options;
					RequestPopup([nodeID, key, options](NodeGraph& g) {
						Node* n = g.FindNode(nodeID);
						if (!n) return false;
						bool picked = false;
						ImGui::TextDisabled("%s", key.c_str());
						ImGui::Separator();
						const bool tall = options.size() > 14;
						if (tall) ImGui::BeginChild("##opts", ImVec2(380.0f, 340.0f));
						int col = 0;
						for (const auto& o : options) {
							if (tall) {
								if (col > 0) ImGui::SameLine(190.0f * col);
								ImGui::SetNextItemWidth(180.0f);
							}
							if (ImGui::Selectable(o.c_str(), n->Properties[key] == o, 0, ImVec2(tall ? 180.0f : 0.0f, 0.0f))) {
								n->Properties[key] = o;
								picked = true;
								ImGui::CloseCurrentPopup();
							}
							if (tall) col = (col + 1) % 2;
						}
						if (tall) ImGui::EndChild();
						return picked;
					});
				}
				const bool hov = ImGui::IsItemHovered();
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(p, ImVec2(p.x + inner, p.y + h), U32(hov ? ImVec4(0.30f, 0.30f, 0.30f, 1) : ImVec4(0.16f, 0.16f, 0.16f, 1)), 4.0f);
				dl->AddText(ImVec2(p.x + 6, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), U32(ImVec4(0.9f, 0.9f, 0.9f, 1)), current.c_str());
				const float ax = p.x + inner - 12.0f, ay = p.y + h * 0.5f;
				dl->AddTriangleFilled(ImVec2(ax - 3, ay - 2), ImVec2(ax + 3, ay - 2), ImVec2(ax, ay + 2), U32(ImVec4(0.8f, 0.8f, 0.8f, 1)));
				ImGui::PopID();
				ImGui::SetCursorScreenPos(ImVec2(origin.x, y + h + 2.0f));
			}
			for (const auto& key : info->Toggles) {
				ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, ImGui::GetCursorScreenPos().y + 2.0f));
				bool v = node.Properties[key] == "1";
				ImGui::PushID(key.c_str());
				if (ImGui::Checkbox(key.c_str(), &v)) { node.Properties[key] = v ? "1" : "0"; changed = true; }
				ImGui::PopID();
				ImGui::SetCursorScreenPos(ImVec2(origin.x, ImGui::GetCursorScreenPos().y));
			}
		}

		// ---- Exposed parameter name (Value / RGB nodes) ----
		if (auto it = node.Properties.find("Name"); it != node.Properties.end()) {
			ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, ImGui::GetCursorScreenPos().y + 2.0f));
			char buf[64];
			strncpy_s(buf, it->second.c_str(), sizeof(buf) - 1);
			ImGui::SetNextItemWidth(inner);
			if (ImGui::InputTextWithHint("##pname", "parameter name", buf, sizeof(buf))) { it->second = buf; changed = true; }
			ImGui::SetCursorScreenPos(ImVec2(origin.x, ImGui::GetCursorScreenPos().y));
		}

		// ---- Custom body ----
		if (DrawNodeBody) {
			ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, ImGui::GetCursorScreenPos().y + 2.0f));
			ImGui::BeginGroup();
			ImGui::PushItemWidth(inner);
			if (DrawNodeBody(node)) changed = true;
			ImGui::PopItemWidth();
			ImGui::EndGroup();
			ImGui::SetCursorScreenPos(ImVec2(origin.x, ImGui::GetCursorScreenPos().y));
		}

		// ---- Inputs ----
		for (auto& pin : node.Inputs) {
			if (!IsPinVisible(info, node, pin)) continue;
			if (hideUnused && !linked.count(pin.PinID)) continue;
			const float y = ImGui::GetCursorScreenPos().y;
			const bool isLinked = linked.count(pin.PinID) > 0;
			socketRow(pin, false, y);
			ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, y + 1.0f));
			ImGui::PushID(static_cast<int>(pin.PinID));
			const bool editable = !isLinked && !pin.HideValue && pin.Type != PinType::Shader && pin.Type != PinType::Flow &&
				pin.Type != PinType::Entity && pin.Type != PinType::Any;
			if (editable) {
				ImGui::BeginGroup();   // keeps multi-row editors (vectors) indented
				if (DrawInputValue(node, pin, inner)) changed = true;
				ImGui::EndGroup();
			}
			else {
				ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f));
				ImGui::TextUnformatted(pin.DisplayName().c_str());
			}
			ImGui::PopID();
			const float next = std::max(ImGui::GetCursorScreenPos().y, y + rowH);
			ImGui::SetCursorScreenPos(ImVec2(origin.x, next));
		}

		// ---- Preview (Node Wrangler style thumbnail of what the node outputs) ----
		if (DrawNodePreview && !(node.Properties.count("__nopreview") && node.Properties.at("__nopreview") == "1")) {
			ImGui::SetCursorScreenPos(ImVec2(origin.x + kMargin, ImGui::GetCursorScreenPos().y + 4.0f));
			ImGui::BeginGroup();
			DrawNodePreview(node, inner);
			ImGui::EndGroup();
			ImGui::SetCursorScreenPos(ImVec2(origin.x, ImGui::GetCursorScreenPos().y));
		}

		ImGui::Dummy(ImVec2(W, 6.0f));
		ImGui::PopID();
		ed::EndNode();

		// ---- Header band (behind the content) ----
		if (ImGui::IsItemVisible()) {
			ImVec4 color = (info && info->HeaderColor.a > 0.0f)
				? ImVec4(info->HeaderColor.r, info->HeaderColor.g, info->HeaderColor.b, 1.0f)
				: CategoryColor(info ? info->Category : "");
			ImDrawList* bg = ed::GetNodeBackgroundDrawList(ed::NodeId(node.NodeID));
			const ImVec2 nmin = ImGui::GetItemRectMin(), nmax = ImGui::GetItemRectMax();
			const float half = ed::GetStyle().NodeBorderWidth * 0.5f;
			bg->AddRectFilled(ImVec2(nmin.x + half, nmin.y + half), ImVec2(nmax.x - half, headerBottom),
				U32(color), ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
		}
	}

	// ============================================================================
	// INTERACTION
	// ============================================================================

	void NodeGraphEditor::HandleCreation(NodeGraph& graph, bool& changed) {
		const ImVec4 dragColor = m_DetachSource ? ImVec4(0, 0, 0, 0) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		const bool creating = ed::BeginCreate(dragColor, 2.2f);
		if (creating) {
			ed::PinId a, b, start;
			const bool hasLink = ed::QueryNewLink(&a, &b);
			const bool hasNode = !hasLink && ed::QueryNewNode(&start);
			const ID startPin = hasLink ? a.Get() : (hasNode ? start.Get() : 0);

			// First frame of the drag: grabbing a connected input carries its link (Blender)
			if (!m_Creating && startPin) {
				m_Creating = true;
				m_DragStartPin = startPin;
				const Pin* sp = graph.FindPin(startPin);
				if (sp && sp->Kind == PinKind::Input) {
					if (const Link* l = graph.GetLinkInto(startPin)) {
						m_DetachSource = l->FromPin;
						graph.RemoveLink(l->LinkID);
						changed = true;
					}
				}
			}

			if (hasLink && a && b) {
				const ID target = b.Get();
				if (m_DetachSource) {
					// Re-route the carried link to the hovered input (or keep it on a new source)
					const Pin* tp = graph.FindPin(target);
					const bool ok = tp && ((tp->Kind == PinKind::Input && graph.CanConnect(m_DetachSource, target)) ||
						(tp->Kind == PinKind::Output && graph.CanConnect(target, m_DragStartPin)));
					if (ok) {
						if (ed::AcceptNewItem(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), 2.5f)) {
							if (tp->Kind == PinKind::Input) graph.Connect(m_DetachSource, target);
							else graph.Connect(target, m_DragStartPin);
							changed = true;
						}
					}
					else {
						ed::RejectNewItem(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), 2.0f);
					}
				}
				else if (graph.CanConnect(a.Get(), b.Get())) {
					if (ed::AcceptNewItem(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), 2.5f)) {
						if (graph.Connect(a.Get(), b.Get())) changed = true;
					}
				}
				else {
					ed::RejectNewItem(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), 2.0f);
				}
			}
			else if (hasNode && start && !m_DetachSource) {
				// Released on empty space: add menu, the new node gets connected
				if (ed::AcceptNewItem()) {
					m_PendingLinkPin = start.Get();
					m_SpawnPos = ImGui::GetMousePos();
					m_OpenAddMenu = true;
				}
			}

			// Preview of the carried link, from its source socket to the mouse
			if (m_DetachSource) {
				auto it = m_SocketPos.find(m_DetachSource);
				const Pin* src = graph.FindPin(m_DetachSource);
				if (it != m_SocketPos.end() && src) {
					const ImVec2 p0 = it->second, p1 = ImGui::GetMousePos();
					const float dx = std::max(40.0f, std::abs(p1.x - p0.x) * 0.5f);
					ImGui::GetWindowDrawList()->AddBezierCubic(p0, ImVec2(p0.x + dx, p0.y), ImVec2(p1.x - dx, p1.y), p1,
						U32(PinTypeColor(src->Type)), 2.2f);
				}
			}
			ed::EndCreate();
		}

		if (!creating && m_Creating) {
			m_Creating = false;
			m_DragStartPin = 0;
			m_DetachSource = 0;
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
				const NodeTypeInfo* info = node ? InfoOf(graph, *node) : nullptr;
				if (info && info->Unique) { ed::RejectDeletedItem(); continue; }
				if (ed::AcceptDeletedItem() && graph.RemoveNode(nodeId.Get())) changed = true;
			}
			ed::EndDelete();
		}
	}

	// ============================================================================
	// REROUTES / CANVAS TOOLS
	// ============================================================================

	void NodeGraphEditor::DrawReroute(NodeGraph& graph, Node& node, bool linked) {
		(void)graph;
		ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
		ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0, 0, 0, 0));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0, 0, 0, 0));
		ed::BeginNode(ed::NodeId(node.NodeID));
		const ImVec2 o = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(16.0f, 16.0f));
		const ImVec2 c(o.x + 8.0f, o.y + 8.0f);
		if (!node.Inputs.empty()) {
			ed::BeginPin(ed::PinId(node.Inputs[0].PinID), ed::PinKind::Input);
			ed::PinPivotRect(c, c);
			ed::PinRect(ImVec2(c.x - 8.0f, c.y - 8.0f), ImVec2(c.x, c.y + 8.0f));
			ed::EndPin();
			m_SocketPos[node.Inputs[0].PinID] = c;
		}
		if (!node.Outputs.empty()) {
			ed::BeginPin(ed::PinId(node.Outputs[0].PinID), ed::PinKind::Output);
			ed::PinPivotRect(c, c);
			ed::PinRect(ImVec2(c.x, c.y - 8.0f), ImVec2(c.x + 8.0f, c.y + 8.0f));
			ed::EndPin();
			m_SocketPos[node.Outputs[0].PinID] = c;
		}
		const PinType type = node.Inputs.empty() ? PinType::Any : node.Inputs[0].Type;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddCircleFilled(c, 6.0f, U32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f)));
		dl->AddCircleFilled(c, 5.0f, U32(PinTypeColor(type)));
		if (!linked) dl->AddCircleFilled(c, 2.0f, U32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f)));
		ed::EndNode();
		ed::PopStyleColor(2);
		ed::PopStyleVar(2);
	}

	void NodeGraphEditor::LinkCurve(const Link& link, std::vector<ImVec2>& pts, int segments) const {
		pts.clear();
		auto a = m_SocketPos.find(link.FromPin), b = m_SocketPos.find(link.ToPin);
		if (a == m_SocketPos.end() || b == m_SocketPos.end()) return;
		const ImVec2 p0 = a->second, p3 = b->second;
		// imgui-node-editor eases the tangent strength for short links
		float s = ed::GetStyle().LinkStrength;
		const float dx = p3.x - p0.x, dy = p3.y - p0.y;
		const float half = std::sqrt(dx * dx + dy * dy) * 0.5f;
		if (half < s) s = s * std::sin(IM_PI * 0.5f * half / s);
		const ImVec2 p1(p0.x + s, p0.y), p2(p3.x - s, p3.y);
		for (int i = 0; i <= segments; i++) {
			const float t = static_cast<float>(i) / segments, u = 1.0f - t;
			const float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
			pts.push_back(ImVec2(w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x, w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y));
		}
	}

	namespace {
		bool SegmentsIntersect(const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, ImVec2* at) {
			const float r_x = b.x - a.x, r_y = b.y - a.y, s_x = d.x - c.x, s_y = d.y - c.y;
			const float den = r_x * s_y - r_y * s_x;
			if (std::abs(den) < 1e-6f) return false;
			const float t = ((c.x - a.x) * s_y - (c.y - a.y) * s_x) / den;
			const float u = ((c.x - a.x) * r_y - (c.y - a.y) * r_x) / den;
			if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) return false;
			if (at) *at = ImVec2(a.x + t * r_x, a.y + t * r_y);
			return true;
		}
	}

	void NodeGraphEditor::HandleCanvasTools(NodeGraph& graph, bool& changed) {
		const ImGuiIO& io = ImGui::GetIO();
		const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const ImVec2 mouse = ImGui::GetMousePos();   // canvas space inside ed::Begin/End
		const bool onEmpty = !ed::GetHoveredNode() && !ed::GetHoveredPin() && !ed::GetHoveredLink();

		// ---- Box selection (drawn by us so it stays above the nodes) ----
		if (hovered && onEmpty && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !m_Creating) {
			m_BoxSelecting = true;
			m_BoxStart = mouse;
		}
		if (m_BoxSelecting) {
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || m_Creating) m_BoxSelecting = false;
			else { m_ScreenBoxA = ed::CanvasToScreen(m_BoxStart); m_ScreenBoxB = ed::CanvasToScreen(mouse); }
		}

		// ---- Ctrl + right drag: cut links (knife), Shift + right drag: add reroutes ----
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && (io.KeyCtrl || io.KeyShift) && !io.KeyAlt) {
			m_Stroke = io.KeyCtrl ? Stroke::Knife : Stroke::Reroute;
			m_StrokePoints = { mouse };
		}
		if (m_Stroke != Stroke::None) {
			m_SuppressContextMenu = true;
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				const ImVec2 last = m_StrokePoints.back();
				if ((mouse.x - last.x) * (mouse.x - last.x) + (mouse.y - last.y) * (mouse.y - last.y) > 16.0f) m_StrokePoints.push_back(mouse);
				for (const auto& p : m_StrokePoints) m_ScreenStroke.push_back(ed::CanvasToScreen(p));
				m_ScreenStroke.push_back(ed::CanvasToScreen(mouse));
			}
			else {
				m_StrokePoints.push_back(mouse);
				struct Hit { ID LinkID, From, To; ImVec2 At; };
				std::vector<Hit> hits;
				std::vector<ImVec2> curve;
				for (const auto& link : graph.GetLinks()) {
					LinkCurve(link, curve);
					bool found = false;
					ImVec2 at;
					for (size_t i = 1; i < curve.size() && !found; i++)
						for (size_t j = 1; j < m_StrokePoints.size() && !found; j++)
							found = SegmentsIntersect(curve[i - 1], curve[i], m_StrokePoints[j - 1], m_StrokePoints[j], &at);
					if (found) hits.push_back({ link.LinkID, link.FromPin, link.ToPin, at });
				}
				if (m_Stroke == Stroke::Knife) {
					for (const auto& h : hits) if (graph.RemoveLink(h.LinkID)) changed = true;
				}
				else {
					for (const auto& h : hits) {
						Node* reroute = graph.AddNode(RerouteType, { h.At.x - 8.0f, h.At.y - 8.0f });
						if (!reroute) continue;
						const ID rID = reroute->NodeID;
						const ID rIn = reroute->Inputs[0].PinID, rOut = reroute->Outputs[0].PinID;
						ed::SetNodePosition(ed::NodeId(rID), ImVec2(h.At.x - 8.0f, h.At.y - 8.0f));
						graph.Connect(h.From, rIn);
						graph.Connect(rOut, h.To);
						changed = true;
					}
				}
				m_Stroke = Stroke::None;
				m_StrokePoints.clear();
			}
		}

		// ---- Drag a node without links over a link: it gets inserted on release (Blender auto-offset) ----
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) m_PressNode = (hovered && ed::GetHoveredNode()) ? ed::GetHoveredNode().Get() : 0;
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (m_PressNode && m_InsertLink) {
				const Link* l = nullptr;
				for (const auto& x : graph.GetLinks()) if (x.LinkID == m_InsertLink) { l = &x; break; }
				Node* n = graph.FindNode(m_PressNode);
				if (l && n) {
					const ID from = l->FromPin, to = l->ToPin;
					const Pin* fromPin = graph.FindPin(from);
					const Pin* toPin = graph.FindPin(to);
					const NodeTypeInfo* info = InfoOf(graph, *n);
					auto pick = [&](std::vector<Pin>& pins, bool inputs) -> ID {
						for (int pass = 0; pass < 2; pass++)
							for (const auto& p : pins) {
								if (!IsPinVisible(info, *n, p) || p.Name == NewSocketPin) continue;
								const bool exact = inputs ? p.Type == fromPin->Type : p.Type == toPin->Type;
								if (pass == 0 && !exact) continue;
								if (inputs ? graph.CanConnect(from, p.PinID) : graph.CanConnect(p.PinID, to)) return p.PinID;
							}
						return 0;
					};
					const ID in = fromPin && toPin ? pick(n->Inputs, true) : 0;
					const ID out = fromPin && toPin ? pick(n->Outputs, false) : 0;
					if (in && out) {
						graph.Connect(from, in);
						graph.Connect(out, to);
						changed = true;
						// Auto-offset (Blender): push the nodes on the right to make room for the inserted one
						const ImVec2 npos = ed::GetNodePosition(ed::NodeId(n->NodeID));
						const ImVec2 nsize = ed::GetNodeSize(ed::NodeId(n->NodeID));
						if (Node* target = graph.FindPinOwner(to)) {
							const float push = (npos.x + nsize.x + 40.0f) - target->Position.x;
							if (push > 0.0f) {
								const float fromX = target->Position.x - 1.0f;
								for (auto& other : graph.GetNodes()) {
									if (other.NodeID == n->NodeID || other.Position.x < fromX) continue;
									other.Position.x += push;
									ed::SetNodePosition(ed::NodeId(other.NodeID), ImVec2(other.Position.x, other.Position.y));
								}
							}
						}
					}
				}
			}
			m_PressNode = 0;
			m_InsertLink = 0;
		}
		else if (m_PressNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f) && !io.KeyAlt) {
			m_InsertLink = 0;
			const Node* n = graph.FindNode(m_PressNode);
			bool free = n != nullptr;
			if (n) {
				for (const auto& l : graph.GetLinks()) {
					for (const auto& p : n->Inputs) if (l.ToPin == p.PinID) free = false;
					for (const auto& p : n->Outputs) if (l.FromPin == p.PinID) free = false;
				}
			}
			if (free) {
				const ImVec2 pos = ed::GetNodePosition(ed::NodeId(m_PressNode));
				const ImVec2 size = ed::GetNodeSize(ed::NodeId(m_PressNode));
				std::vector<ImVec2> curve;
				for (const auto& l : graph.GetLinks()) {
					LinkCurve(l, curve);
					bool inside = false;
					for (const auto& p : curve)
						if (p.x > pos.x && p.x < pos.x + size.x && p.y > pos.y && p.y < pos.y + size.y) { inside = true; break; }
					if (!inside) continue;
					m_InsertLink = l.LinkID;
					for (const auto& p : curve) m_ScreenInsert.push_back(ed::CanvasToScreen(p));
					break;
				}
			}
		}
	}

	void NodeGraphEditor::HandleShortcuts(NodeGraph& graph, bool& changed) {
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput || !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			return;

		std::vector<ed::NodeId> selected(static_cast<size_t>(std::max(ed::GetSelectedObjectCount(), 0)));
		const int count = selected.empty() ? 0 : ed::GetSelectedNodes(selected.data(), static_cast<int>(selected.size()));
		selected.resize(static_cast<size_t>(count));

		// Shift+A: add menu at the mouse
		if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
			m_SpawnPos = ImGui::GetMousePos();
			m_PendingLinkPin = 0;
			m_OpenAddMenu = true;
		}
		// X: delete selection (Delete is handled by the node editor itself)
		if (!io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
			for (auto id : selected) ed::DeleteNode(id);
		}
		// Shift+D: duplicate selection
		if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D, false) && !selected.empty()) {
			ed::ClearSelection();
			for (auto id : selected) {
				if (Node* copy = graph.DuplicateNode(id.Get(), glm::vec2(30.0f, 30.0f))) {
					ed::SetNodePosition(ed::NodeId(copy->NodeID), ImVec2(copy->Position.x, copy->Position.y));
					ed::SelectNode(ed::NodeId(copy->NodeID), true);
					changed = true;
				}
			}
		}
		// ---- Node Wrangler ----
		std::vector<ID> ids;
		for (auto id : selected) ids.push_back(id.Get());
		// F / Shift+F: make links between the selected nodes (Shift replaces existing links)
		if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false) && ids.size() >= 2) {
			if (MakeLinks(graph, ids, io.KeyShift)) changed = true;
		}
		// Ctrl+X: dissolve (delete and keep the flow connected)
		if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
			for (ID id : ids) if (DissolveNode(graph, id)) changed = true;
		}
		// Alt+S: swap the links of the first two connected inputs
		if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			for (ID id : ids) if (SwapInputs(graph, id)) changed = true;
		}
		// Ctrl+H: hide / show unused sockets
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_H, false)) {
			for (ID id : ids) if (Node* n = graph.FindNode(id)) {
				std::string& h = n->Properties["__hide"];
				h = h == "1" ? "0" : "1";
				changed = true;
			}
		}
		// Ctrl+T / Ctrl+Shift+T: texture setups (domain specific, e.g. materials)
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_T, false) && ids.size() == 1) {
			if (io.KeyShift) { if (OnPrincipledTextureSetup && OnPrincipledTextureSetup(graph, ids[0])) changed = true; }
			else if (OnTextureSetup && OnTextureSetup(graph, ids[0])) changed = true;
			m_PositionsDirty = true;
		}
		// Node groups: Ctrl+G make (and enter), Tab enter / exit, Ctrl+Alt+G ungroup
		if (EnableGroups) {
			if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
				Node* sel = ids.size() == 1 ? graph.FindNode(ids[0]) : nullptr;
				if (!io.KeyCtrl && sel && sel->IsGroup()) { m_GroupPath.push_back(sel->NodeID); ed::ClearSelection(); }
				else if (!m_GroupPath.empty()) { m_GroupPath.pop_back(); ed::ClearSelection(); }
			}
			if (io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_G, false) && !ids.empty()) {
				if (const ID group = graph.MakeGroup(ids)) {
					changed = true;
					ed::ClearSelection();
					m_PositionsDirty = true;
					m_GroupPath.push_back(group);
				}
			}
			if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
				for (ID id : ids) if (const Node* n = graph.FindNode(id); n && n->IsGroup() && graph.Ungroup(id)) changed = true;
				ed::ClearSelection();
				m_PositionsDirty = true;
			}
		}

		// Home: frame everything, Period: frame selection (Blender)
		if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) ed::NavigateToContent();
		if (ImGui::IsKeyPressed(ImGuiKey_Period, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false)) {
			if (!selected.empty()) ed::NavigateToSelection(false);
			else ed::NavigateToContent();
		}

		// Ctrl+Shift+Click: preview the node's output (cycles through outputs)
		if (io.KeyCtrl && io.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const ed::NodeId hovered = ed::GetHoveredNode();
			if (hovered && OnPreviewNode) {
				const ID nodeID = hovered.Get();
				m_PreviewOutput = (m_PreviewNode == nodeID) ? m_PreviewOutput + 1 : 0;
				m_PreviewNode = nodeID;
				if (OnPreviewNode(graph, nodeID, m_PreviewOutput)) { changed = true; m_PositionsDirty = true; }
			}
		}

		// Alt + right-drag from one node to another: lazy connect
		if (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			const ed::NodeId hovered = ed::GetHoveredNode();
			m_LazyFrom = hovered ? hovered.Get() : 0;
		}
		if (m_LazyFrom) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				const ImVec2 a = ed::GetNodePosition(ed::NodeId(m_LazyFrom));
				const ImVec2 size = ed::GetNodeSize(ed::NodeId(m_LazyFrom));
				const ImVec2 from(a.x + size.x, a.y + size.y * 0.5f);
				m_ScreenLazy = { ed::CanvasToScreen(from), ed::CanvasToScreen(ImGui::GetMousePos()) };
			}
			else {
				const ed::NodeId target = ed::GetHoveredNode();
				if (target && target.Get() != m_LazyFrom) {
					std::vector<ID> pair = { m_LazyFrom, target.Get() };
					if (MakeLinks(graph, pair, false, true)) changed = true;
				}
				m_LazyFrom = 0;
				m_SuppressContextMenu = true;
			}
		}
	}

	Node* NodeGraphEditor::SpawnNode(NodeGraph& graph, const std::string& typeName) {
		Node* node = graph.AddNode(typeName, { m_SpawnPos.x, m_SpawnPos.y });
		if (!node) return nullptr;
		const ID nodeID = node->NodeID;
		ed::SetNodePosition(ed::NodeId(nodeID), m_SpawnPos);

		// Connect to the pin the link was dragged from (first compatible socket, exact type first)
		if (m_PendingLinkPin) {
			const Pin* from = graph.FindPin(m_PendingLinkPin);
			Node* fresh = graph.FindNode(nodeID);
			if (from && fresh) {
				auto& candidates = from->Kind == PinKind::Output ? fresh->Inputs : fresh->Outputs;
				const Pin* best = nullptr;
				for (const auto& p : candidates) if (p.Type == from->Type) { best = &p; break; }
				if (!best) for (const auto& p : candidates) {
					const bool ok = from->Kind == PinKind::Output ? ArePinTypesCompatible(from->Type, p.Type) : ArePinTypesCompatible(p.Type, from->Type);
					if (ok) { best = &p; break; }
				}
				if (best) graph.Connect(m_PendingLinkPin, best->PinID);
			}
			m_PendingLinkPin = 0;
		}
		return graph.FindNode(nodeID);
	}

	bool NodeGraphEditor::DrawAddMenu(NodeGraph& graph) {
		const NodeRegistry* registry = graph.GetRegistry();
		if (!registry) return false;
		bool added = false;

		// Domain nodes first, then the built-in layout nodes (reroute)
		std::vector<const NodeTypeInfo*> types;
		for (const auto& typeName : registry->GetOrder()) if (const NodeTypeInfo* i = registry->Get(typeName)) types.push_back(i);
		const NodeRegistry& builtin = NodeGraph::GetBuiltinRegistry();
		for (const auto& typeName : builtin.GetOrder()) if (const NodeTypeInfo* i = builtin.Get(typeName)) types.push_back(i);

		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputTextWithHint("##search", "Search...", m_Search, sizeof(m_Search));
		ImGui::Separator();

		auto usable = [&](const NodeTypeInfo* info) {
			return info && !info->HiddenInMenu && !(info->Unique && graph.FindNodeByType(info->TypeName));
		};

		const std::string query = Lower(m_Search);
		if (!query.empty()) {
			int shown = 0;
			for (const NodeTypeInfo* info : types) {
				if (!usable(info) || Lower(info->Title).find(query) == std::string::npos) continue;
				const std::string label = info->Title + "  (" + info->Category + ")";
				if (ImGui::MenuItem(label.c_str()) || (shown == 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
					added = SpawnNode(graph, info->TypeName) != nullptr;
					ImGui::CloseCurrentPopup();
					break;
				}
				shown++;
			}
			if (shown == 0) ImGui::TextDisabled("No match");
			return added;
		}

		// Categories in registration order
		std::vector<std::string> categories;
		for (const NodeTypeInfo* info : types)
			if (usable(info) && std::find(categories.begin(), categories.end(), info->Category) == categories.end())
				categories.push_back(info->Category);
		for (const auto& category : categories) {
			const ImVec4 c = CategoryColor(category);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(std::min(1.0f, c.x + 0.45f), std::min(1.0f, c.y + 0.45f), std::min(1.0f, c.z + 0.45f), 1.0f));
			const bool open = ImGui::BeginMenu(category.c_str());
			ImGui::PopStyleColor();
			if (!open) continue;
			for (const NodeTypeInfo* info : types) {
				if (!usable(info) || info->Category != category) continue;
				if (ImGui::MenuItem(info->Title.c_str())) added = SpawnNode(graph, info->TypeName) != nullptr;
				if (!info->Description.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info->Description.c_str());
			}
			ImGui::EndMenu();
		}
		return added;
	}

	// ============================================================================
	// NODE WRANGLER OPERATIONS
	// ============================================================================

	bool NodeGraphEditor::MakeLinks(NodeGraph& graph, std::vector<ID> nodes, bool replace, bool keepOrder) {
		// Left to right by position (lazy connect keeps from -> to)
		if (!keepOrder) {
			std::sort(nodes.begin(), nodes.end(), [&](ID a, ID b) {
				const Node* na = graph.FindNode(a); const Node* nb = graph.FindNode(b);
				return (na ? na->Position.x : 0.0f) < (nb ? nb->Position.x : 0.0f);
			});
		}
		bool any = false;
		for (size_t i = 0; i + 1 < nodes.size(); i++) {
			Node* from = graph.FindNode(nodes[i]);
			Node* to = graph.FindNode(nodes[i + 1]);
			if (!from || !to) continue;
			const NodeTypeInfo* toInfo = InfoOf(graph, *to);
			// Exact type first, then any compatible type
			for (int pass = 0; pass < 2 && from; pass++) {
				bool done = false;
				for (const auto& out : from->Outputs) {
					for (const auto& in : to->Inputs) {
						if (!IsPinVisible(toInfo, *to, in)) continue;
						if (!replace && graph.GetLinkInto(in.PinID)) continue;
						if (pass == 0 ? out.Type != in.Type : !graph.CanConnect(out.PinID, in.PinID)) continue;
						if (graph.Connect(out.PinID, in.PinID)) { any = done = true; break; }
					}
					if (done) break;
				}
				if (done) break;
			}
		}
		return any;
	}

	bool NodeGraphEditor::DissolveNode(NodeGraph& graph, ID nodeID) {
		const Node* node = graph.FindNode(nodeID);
		if (!node) return false;
		const NodeTypeInfo* info = InfoOf(graph, *node);
		if (info && info->Unique) return false;

		// Upstream source: the first linked input (flow first for blueprints)
		ID source = 0;
		for (int pass = 0; pass < 2 && !source; pass++)
			for (const auto& in : node->Inputs) {
				if (pass == 0 && in.Type != PinType::Flow) continue;
				if (const Link* l = graph.GetLinkInto(in.PinID)) { source = l->FromPin; break; }
			}
		// Downstream targets of every output
		std::vector<ID> targets;
		for (const auto& out : node->Outputs)
			for (const auto& l : graph.GetLinks()) if (l.FromPin == out.PinID) targets.push_back(l.ToPin);

		graph.RemoveNode(nodeID);
		ed::DeleteNode(ed::NodeId(nodeID));
		if (source) for (ID t : targets) if (graph.CanConnect(source, t)) graph.Connect(source, t);
		return true;
	}

	bool NodeGraphEditor::SwapInputs(NodeGraph& graph, ID nodeID) {
		Node* node = graph.FindNode(nodeID);
		if (!node) return false;
		std::vector<std::pair<ID, ID>> linkedInputs;   // input pin, source pin
		for (const auto& in : node->Inputs)
			if (const Link* l = graph.GetLinkInto(in.PinID)) linkedInputs.push_back({ in.PinID, l->FromPin });
		if (linkedInputs.size() < 2) return false;
		const auto a = linkedInputs[0], b = linkedInputs[1];
		if (!graph.CanConnect(b.second, a.first) || !graph.CanConnect(a.second, b.first)) return false;
		graph.Connect(b.second, a.first);
		graph.Connect(a.second, b.first);
		return true;
	}

	void NodeGraphEditor::DrawPopups(NodeGraph& graph, bool& changed) {
		// Mouse position in canvas space must be read before suspending
		const ImVec2 canvasMouse = ImGui::GetMousePos();

		ed::Suspend();

		ed::NodeId contextNode = 0;
		ed::LinkId contextLink = 0;
		const bool suppress = m_SuppressContextMenu || ImGui::GetIO().KeyAlt || m_Stroke != Stroke::None;
		m_SuppressContextMenu = false;
		if (suppress) {
			// Alt+RMB is lazy connect: swallow the menus it would open
			ed::NodeId n; ed::LinkId l;
			ed::ShowNodeContextMenu(&n); ed::ShowLinkContextMenu(&l); ed::ShowBackgroundContextMenu();
		}
		else if (ed::ShowNodeContextMenu(&contextNode)) {
			ImGui::OpenPopup("##NodeContext");
			ImGui::GetStateStorage()->SetInt(ImGui::GetID("##ctxnode"), static_cast<int>(contextNode.Get()));
		}
		else if (ed::ShowLinkContextMenu(&contextLink)) {
			ImGui::OpenPopup("##LinkContext");
			ImGui::GetStateStorage()->SetInt(ImGui::GetID("##ctxlink"), static_cast<int>(contextLink.Get()));
		}
		else if (ed::ShowBackgroundContextMenu()) {
			m_SpawnPos = canvasMouse;
			m_PendingLinkPin = 0;
			m_OpenAddMenu = true;
		}

		if (m_OpenAddMenu) {
			m_Search[0] = '\0';
			ImGui::OpenPopup("##AddNode");
			m_OpenAddMenu = false;
		}
		ImGui::SetNextWindowSizeConstraints(ImVec2(230, 0), ImVec2(420, 520));
		if (ImGui::BeginPopup("##AddNode")) {
			ImGui::TextDisabled("Add");
			if (DrawAddMenu(graph)) changed = true;
			ImGui::EndPopup();
		}
		else {
			m_PendingLinkPin = m_OpenAddMenu ? m_PendingLinkPin : 0;
		}

		if (ImGui::BeginPopup("##NodeContext")) {
			const ID nodeID = static_cast<ID>(ImGui::GetStateStorage()->GetInt(ImGui::GetID("##ctxnode")));
			if (ImGui::MenuItem("Duplicate", "Shift+D")) {
				if (Node* copy = graph.DuplicateNode(nodeID, glm::vec2(30.0f, 30.0f))) {
					ed::SetNodePosition(ed::NodeId(copy->NodeID), ImVec2(copy->Position.x, copy->Position.y));
					changed = true;
				}
			}
			if (ImGui::MenuItem("Delete", "X")) ed::DeleteNode(ed::NodeId(nodeID));
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("##LinkContext")) {
			const ID linkID = static_cast<ID>(ImGui::GetStateStorage()->GetInt(ImGui::GetID("##ctxlink")));
			if (ImGui::MenuItem("Delete Link")) { if (graph.RemoveLink(linkID)) changed = true; }
			ImGui::EndPopup();
		}

		// Generic popup (drop-downs, color pickers, ramp stops...)
		if (m_OpenPopup) {
			ImGui::OpenPopup("##NodeEdPopup");
			m_OpenPopup = false;
		}
		if (ImGui::BeginPopup("##NodeEdPopup")) {
			if (m_PopupContent && m_PopupContent(graph)) changed = true;
			ImGui::EndPopup();
		}
		else {
			m_PopupContent = nullptr;
		}

		ed::Resume();
	}

} // namespace Lunex::UI
