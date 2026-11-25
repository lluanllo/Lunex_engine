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
			return graph.GetRegistry() ? graph.GetRegistry()->Get(node.TypeName) : nullptr;
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
		style.Colors[ed::StyleColor_NodeSelRect] = ImVec4(1.0f, 0.63f, 0.20f, 0.10f);
		style.Colors[ed::StyleColor_NodeSelRectBorder] = ImVec4(1.0f, 0.63f, 0.20f, 0.6f);
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

	bool NodeGraphEditor::Draw(NodeGraph& graph, const char* emptyHint) {
		bool changed = false;
		if (&graph != m_LastGraph) {
			m_LastGraph = &graph;
			m_PositionsDirty = true;
			m_FrameDelay = 2;
			m_SocketPos.clear();
		}

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

		if (graph.GetNodes().empty() && emptyHint) {
			ImVec2 min = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 12, min.y + 12), ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyHint);
		}
		if (changed) graph.Touch();
		return changed;
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
		if (ImGui::ColorButton("##swatch", ImVec4(color.r, color.g, color.b, color.a),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(width, h))) {
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
				ColorField(node.NodeID, pin.PinID, pin.DefaultValue, swatch);
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
		for (auto& pin : node.Outputs) {
			if (!IsPinVisible(info, node, pin)) continue;
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
				const std::string current = node.Properties.count(e.Key) ? node.Properties[e.Key] : (e.Options.empty() ? "" : e.Options[0]);
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
		// F: frame selection (or everything), Home: frame everything
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
			if (!selected.empty()) ed::NavigateToSelection(false);
			else ed::NavigateToContent();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) ed::NavigateToContent();
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
			for (const auto& typeName : registry->GetOrder()) {
				const NodeTypeInfo* info = registry->Get(typeName);
				if (!usable(info) || Lower(info->Title).find(query) == std::string::npos) continue;
				const std::string label = info->Title + "  (" + info->Category + ")";
				if (ImGui::MenuItem(label.c_str()) || (shown == 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
					added = SpawnNode(graph, typeName) != nullptr;
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
		for (const auto& typeName : registry->GetOrder()) {
			const NodeTypeInfo* info = registry->Get(typeName);
			if (usable(info) && std::find(categories.begin(), categories.end(), info->Category) == categories.end())
				categories.push_back(info->Category);
		}
		for (const auto& category : categories) {
			const ImVec4 c = CategoryColor(category);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(std::min(1.0f, c.x + 0.45f), std::min(1.0f, c.y + 0.45f), std::min(1.0f, c.z + 0.45f), 1.0f));
			const bool open = ImGui::BeginMenu(category.c_str());
			ImGui::PopStyleColor();
			if (!open) continue;
			for (const auto& typeName : registry->GetOrder()) {
				const NodeTypeInfo* info = registry->Get(typeName);
				if (!usable(info) || info->Category != category) continue;
				if (ImGui::MenuItem(info->Title.c_str())) added = SpawnNode(graph, typeName) != nullptr;
				if (!info->Description.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info->Description.c_str());
			}
			ImGui::EndMenu();
		}
		return added;
	}

	void NodeGraphEditor::DrawPopups(NodeGraph& graph, bool& changed) {
		// Mouse position in canvas space must be read before suspending
		const ImVec2 canvasMouse = ImGui::GetMousePos();

		ed::Suspend();

		ed::NodeId contextNode = 0;
		ed::LinkId contextLink = 0;
		if (ed::ShowNodeContextMenu(&contextNode)) {
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
