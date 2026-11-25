#include "stpch.h"
#include "MaterialGraphPanel.h"

#include "ContentBrowserPanel.h"
#include "Assets/Materials/MaterialGraph.h"
#include "Assets/Materials/MaterialGraphCompiler.h"
#include "Resources/Mesh/Model.h"
#include "Renderer/Texture.h"
#include "Utils/PlatformUtils.h"
#include "Log/Log.h"

#include "../UI/UICore.h"
#include "../UI/Lunexi/Component.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	using namespace Graph;

	MaterialGraphPanel::MaterialGraphPanel() {
		m_Editor = std::make_unique<UI::NodeGraphEditor>("MaterialNodeEditor");
		m_Editor->DrawNodeBody = [this](Node& node) { return DrawNodeBody(node); };
		m_Preview = CreateRef<MaterialPreviewRenderer>();
		m_Preview->SetResolution(m_PreviewSize, m_PreviewSize);
		m_Preview->SetAutoRotate(true);
	}

	// ============================================================================
	// OPEN / CLOSE
	// ============================================================================

	void MaterialGraphPanel::Open(Ref<MaterialAsset> material) {
		if (!material) return;
		if (m_Material && m_Material != material && m_Dirty) Save();

		if (!material->GetNodeGraph())
			material->SetNodeGraph(MaterialGraph::CreateFromMaterial(*material));
		if (!material->UsesNodeGraph()) material->SetUseNodeGraph(true);

		m_Material = material;
		m_Messages = m_Material->RebuildGraph();
		m_IsOpen = true;
		m_Focus = 3;           // a few frames: other windows opened in the same frame may grab focus
		m_Dirty = false;
		m_Editor->RequestFrameAll();
	}

	void MaterialGraphPanel::Close() {
		if (m_Dirty) Save();
		m_IsOpen = false;
		m_Material.reset();
	}

	void MaterialGraphPanel::Save() {
		if (!m_Material) return;
		if (m_Material->Save()) {
			m_Dirty = false;
			if (m_OnSaved) m_OnSaved(m_Material->GetPath());
		}
		else {
			LNX_LOG_ERROR("Failed to save material {0}", m_Material->GetName());
		}
	}

	void MaterialGraphPanel::OnGraphChanged() {
		m_Messages = m_Material->RebuildGraph();
		m_Material->MarkDirty();
		m_Dirty = true;
	}

	void MaterialGraphPanel::OnUpdate(float deltaTime) {
		if (!IsOpen()) return;
		m_Preview->Update(deltaTime);
		m_Preview->RenderPreview(m_Material);
	}

	// ============================================================================
	// WINDOW
	// ============================================================================

	void MaterialGraphPanel::OnImGuiRender() {
		if (!IsOpen()) return;

		std::string title = "Node Editor - " + m_Material->GetName() + (m_Dirty ? " *" : "") + "###MaterialGraphPanel";
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

			DrawToolbar();

			const float footer = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
			ImGui::BeginChild("##body", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			UI::Layouts::SplitProps split;
			split.ID = "MatGraphSplit";
			split.Ratio = &m_SidebarRatio;
			split.MinLeft = 200.0f;
			split.MinRight = 300.0f;
			UI::Slots slots;
			slots.Named["Left"] = [this] { DrawSidebar(); };
			slots.Named["Right"] = [this] {
				auto graph = m_Material->GetNodeGraph();
				if (graph && m_Editor->Draw(*graph, "Shift+A or right-click to add nodes")) OnGraphChanged();
			};
			UI::Layouts::Split(split, slots);
			ImGui::EndChild();

			// Shortcut hints (Blender status bar)
			ImGui::SetCursorPos(ImVec2(10.0f, ImGui::GetCursorPosY() + 3.0f));
			ImGui::TextDisabled("Shift+A Add   |   Drag a connected input: detach   |   X / Del Delete   |   Shift+D Duplicate   |   F Frame   |   Middle mouse Pan   |   Ctrl+S Save");
		}
		ImGui::End();
		if (!open) Close();
	}

	void MaterialGraphPanel::DrawToolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::Colors::BgDark().ToImVec4());
		ImGui::BeginChild("##toolbar", ImVec2(0, ImGui::GetFrameHeight() + 10.0f), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);

		if (ImGui::Button("Save")) Save();
		ImGui::SameLine();
		if (ImGui::Button("Frame All")) m_Editor->RequestFrameAll();
		ImGui::SameLine();
		if (ImGui::Button("Material Properties") && m_OnOpenProperties) m_OnOpenProperties(m_Material);
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();

		// Compile status
		auto* runtime = m_Material->GetGraphRuntime();
		const bool shaderError = runtime && runtime->HasShaderError();
		if (!m_Messages.empty() || shaderError) {
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s", shaderError ? "Shader error (see console)" : m_Messages.front().c_str());
		}
		else {
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Compiled");
			if (runtime) {
				ImGui::SameLine();
				ImGui::TextDisabled("%d values, %d textures", static_cast<int>(runtime->GetCompiled().Params.size()), static_cast<int>(runtime->GetCompiled().TexturePaths.size()));
			}
		}

		// Preview controls (right side)
		const float right = ImGui::GetWindowContentRegionMax().x;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 20.0f, right - 290.0f));
		ImGui::TextDisabled("Preview");
		ImGui::SameLine();
		static const char* shapes[] = { "Sphere", "Cube", "Plane", "Cylinder" };
		static int shape = 0;
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::Combo("##shape", &shape, shapes, IM_ARRAYSIZE(shapes))) {
			switch (shape) {
				case 0: m_Preview->SetPreviewModel(Model::CreateSphere()); break;
				case 1: m_Preview->SetPreviewModel(Model::CreateCube()); break;
				case 2: m_Preview->SetPreviewModel(Model::CreatePlane()); break;
				default: m_Preview->SetPreviewModel(Model::CreateCylinder()); break;
			}
		}
		ImGui::SameLine();
		bool rotate = m_Preview->GetAutoRotate();
		if (ImGui::Checkbox("Rotate", &rotate)) m_Preview->SetAutoRotate(rotate);

		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	// ============================================================================
	// SIDEBAR: preview + details
	// ============================================================================

	void MaterialGraphPanel::DrawSidebar() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		ImGui::BeginChild("##sidebar", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

		// Square preview
		const float w = ImGui::GetContentRegionAvail().x;
		const uint32_t size = static_cast<uint32_t>(std::max(64.0f, w));
		if (size != m_PreviewSize) { m_PreviewSize = size; m_Preview->SetResolution(size, size); }
		if (uint32_t tex = m_Preview->GetPreviewTextureID())
			ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(w, w), ImVec2(0, 1), ImVec2(1, 0));
		else
			ImGui::Dummy(ImVec2(w, w));

		ImGui::Spacing();
		if (auto graph = m_Material->GetNodeGraph()) DrawDetails(*graph);

		if (!m_Messages.empty()) {
			ImGui::Spacing();
			ImGui::SeparatorText("Messages");
			for (const auto& m : m_Messages) ImGui::TextWrapped("! %s", m.c_str());
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	void MaterialGraphPanel::DrawDetails(NodeGraph& graph) {
		ImGui::SeparatorText("Node");
		Node* node = graph.FindNode(m_Editor->GetSelectedNode());
		if (!node) {
			ImGui::TextDisabled("Select a node to edit it here.");
			ImGui::Spacing();
			ImGui::SeparatorText("Material");
			ImGui::TextDisabled("Nodes: %d   Links: %d", static_cast<int>(graph.GetNodes().size()), static_cast<int>(graph.GetLinks().size()));
			ImGui::TextWrapped("Name a Value or RGB node to change it per entity from scripts (Material.SetParameter).");
			return;
		}

		const NodeTypeInfo* info = graph.GetRegistry() ? graph.GetRegistry()->Get(node->TypeName) : nullptr;
		ImGui::TextUnformatted(node->Title.c_str());
		if (info && !info->Description.empty()) ImGui::TextDisabled("%s", info->Description.c_str());
		ImGui::Spacing();

		bool changed = false;
		const float width = ImGui::GetContentRegionAvail().x;
		ImGui::PushID(static_cast<int>(node->NodeID));
		if (info) {
			for (const auto& e : info->Enums) {
				std::string& value = node->Properties[e.Key];
				ImGui::SetNextItemWidth(width);
				if (ImGui::BeginCombo(("##" + e.Key).c_str(), value.c_str())) {
					for (const auto& o : e.Options)
						if (ImGui::Selectable(o.c_str(), o == value)) { value = o; changed = true; }
					ImGui::EndCombo();
				}
			}
			for (const auto& key : info->Toggles) {
				bool v = node->Properties[key] == "1";
				if (ImGui::Checkbox(key.c_str(), &v)) { node->Properties[key] = v ? "1" : "0"; changed = true; }
			}
		}
		if (node->TypeName == MaterialGraph::ValueNode || node->TypeName == MaterialGraph::FloatNode)
			changed |= UI::NodeGraphEditor::SliderField("##value", "Value", &node->Value.x, 0.0f, 0.0f, width);
		if (node->TypeName == MaterialGraph::RGBNode || node->TypeName == MaterialGraph::ColorNode) {
			ImGui::SetNextItemWidth(width);
			changed |= ImGui::ColorPicker4("##rgb", glm::value_ptr(node->Value), ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview);
		}

		for (auto& pin : node->Inputs) {
			if (graph.GetLinkInto(pin.PinID) || pin.HideValue) continue;
			if (info && info->PinVisible && !info->PinVisible(*node, pin)) continue;
			ImGui::PushID(static_cast<int>(pin.PinID));
			switch (pin.Type) {
				case PinType::Float:
					changed |= UI::NodeGraphEditor::SliderField("##f", pin.DisplayName().c_str(), &pin.DefaultValue.x, pin.Range.x, pin.Range.y, width);
					break;
				case PinType::Color:
				case PinType::Texture:
					ImGui::SetNextItemWidth(width);
					changed |= ImGui::ColorEdit4(pin.DisplayName().c_str(), glm::value_ptr(pin.DefaultValue), ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_Float);
					break;
				case PinType::Vec3:
				case PinType::Vec2:
					ImGui::SetNextItemWidth(width);
					changed |= ImGui::DragFloat3(pin.DisplayName().c_str(), glm::value_ptr(pin.DefaultValue), 0.01f);
					break;
				default:
					break;
			}
			ImGui::PopID();
		}
		ImGui::PopID();
		if (changed) { graph.Touch(); OnGraphChanged(); }
	}

	// ============================================================================
	// NODE BODIES
	// ============================================================================

	Ref<Texture2D> MaterialGraphPanel::GetThumbnail(const std::string& path) {
		if (path.empty()) return nullptr;
		auto& slot = m_Thumbnails[path];
		if (!slot) slot = Texture2D::Create(path);
		return (slot && slot->IsLoaded()) ? slot : nullptr;
	}

	bool MaterialGraphPanel::DrawNodeBody(Node& node) {
		const std::string& t = node.TypeName;
		const float width = ImGui::CalcItemWidth();
		if (t == MaterialGraph::ImageTexture || t == MaterialGraph::TextureNode) return DrawImageBody(node);
		if (t == MaterialGraph::ColorRampNode) return DrawRampBody(node);
		if (t == MaterialGraph::ValueNode || t == MaterialGraph::FloatNode)
			return UI::NodeGraphEditor::SliderField("##value", "", &node.Value.x, 0.0f, 0.0f, width);
		if (t == MaterialGraph::RGBNode || t == MaterialGraph::ColorNode) {
			m_Editor->ColorField(node.NodeID, 0, node.Value, width, 46.0f);
			return false;
		}
		return false;
	}

	bool MaterialGraphPanel::DrawImageBody(Node& node) {
		bool changed = false;
		std::string& path = node.Properties["Path"];
		const float width = ImGui::CalcItemWidth();
		const float size = 64.0f;

		const ImVec2 p = ImGui::GetCursorScreenPos();
		if (Ref<Texture2D> thumb = GetThumbnail(path))
			ImGui::Image((ImTextureID)(intptr_t)thumb->GetRendererID(), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
		else {
			ImGui::InvisibleButton("##drop", ImVec2(size, size));
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size), IM_COL32(40, 40, 40, 255), 4.0f);
			dl->AddRect(p, ImVec2(p.x + size, p.y + size), IM_COL32(90, 90, 90, 255), 4.0f);
			dl->AddText(ImVec2(p.x + 12, p.y + size * 0.5f - 7), IM_COL32(150, 150, 150, 255), "Drop");
		}
		// Drag a texture from the Content Browser onto the node
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				auto* data = static_cast<const ContentBrowserPayload*>(payload->Data);
				std::string ext = data->Extension;
				if (!data->IsDirectory && (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".hdr" || ext == ".bmp")) {
					path = data->FilePath;
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SetCursorScreenPos(ImVec2(p.x + size + 8.0f, p.y));
		ImGui::BeginGroup();
		std::string name = path.empty() ? std::string("No image") : std::filesystem::path(path).filename().string();
		const size_t maxChars = static_cast<size_t>(std::max(6.0f, (width - size - 8.0f) / 7.0f));
		if (name.size() > maxChars) name = name.substr(0, maxChars - 3) + "...";
		ImGui::TextUnformatted(name.c_str());
		if (ImGui::Button("Open")) {
			std::string file = FileDialogs::OpenFile("Images (*.png;*.jpg;*.tga;*.bmp)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
			if (!file.empty()) { path = file; changed = true; }
		}
		if (!path.empty()) {
			ImGui::SameLine();
			if (ImGui::Button("X")) { path.clear(); changed = true; }
		}
		ImGui::EndGroup();
		ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size + 4.0f));
		ImGui::Dummy(ImVec2(width, 0.0f));
		return changed;
	}

	bool MaterialGraphPanel::DrawRampBody(Node& node) {
		bool changed = false;
		auto stops = MaterialGraph::ParseRamp(node.Properties["Stops"]);
		int selected = std::clamp(std::atoi(node.Properties["Selected"].c_str()), 0, static_cast<int>(stops.size()) - 1);
		const bool constant = node.Properties["Interpolation"] == "Constant";
		const float width = ImGui::CalcItemWidth();
		const float barH = 20.0f, markerH = 9.0f;

		// Buttons: add / remove stop
		if (ImGui::Button("+")) {
			const float pos = selected + 1 < static_cast<int>(stops.size()) ? (stops[selected].Position + stops[selected + 1].Position) * 0.5f
				: std::min(1.0f, stops[selected].Position + 0.1f);
			stops.push_back({ pos, stops[selected].Color });
			selected = static_cast<int>(stops.size()) - 1;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("-") && stops.size() > 2) {
			stops.erase(stops.begin() + selected);
			selected = std::max(0, selected - 1);
			changed = true;
		}

		// Gradient bar
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		auto col = [](const glm::vec4& c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1.0f)); };
		std::vector<MaterialGraph::RampStop> sorted = stops;
		std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.Position < b.Position; });
		float x0 = p.x;
		glm::vec4 c0 = sorted.front().Color;
		for (const auto& s : sorted) {
			const float x1 = p.x + std::clamp(s.Position, 0.0f, 1.0f) * width;
			if (x1 > x0) {
				if (constant) dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1, p.y + barH), col(c0));
				else dl->AddRectFilledMultiColor(ImVec2(x0, p.y), ImVec2(x1, p.y + barH), col(c0), col(s.Color), col(s.Color), col(c0));
			}
			x0 = x1; c0 = s.Color;
		}
		if (x0 < p.x + width) dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(p.x + width, p.y + barH), col(c0));
		dl->AddRect(p, ImVec2(p.x + width, p.y + barH), IM_COL32(20, 20, 20, 255));

		// Stop markers (drag to move, click on the bar to select the closest)
		ImGui::InvisibleButton("##ramp", ImVec2(width, barH + markerH));
		const bool active = ImGui::IsItemActive();
		if (ImGui::IsItemActivated()) {
			float best = 1e9f;
			const float mx = ImGui::GetMousePos().x;
			for (int i = 0; i < static_cast<int>(stops.size()); i++) {
				const float d = std::abs(p.x + stops[i].Position * width - mx);
				if (d < best) { best = d; selected = i; }
			}
			changed = true;
		}
		if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
			stops[selected].Position = std::clamp((ImGui::GetMousePos().x - p.x) / width, 0.0f, 1.0f);
			changed = true;
		}
		for (int i = 0; i < static_cast<int>(stops.size()); i++) {
			const float x = p.x + stops[i].Position * width;
			const float y = p.y + barH;
			const ImU32 outline = i == selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255);
			dl->AddTriangleFilled(ImVec2(x, y), ImVec2(x - 5, y + markerH), ImVec2(x + 5, y + markerH), col(stops[i].Color));
			dl->AddTriangle(ImVec2(x, y), ImVec2(x - 5, y + markerH), ImVec2(x + 5, y + markerH), outline, 1.5f);
		}

		// Selected stop: position + color
		float pos = stops[selected].Position;
		if (UI::NodeGraphEditor::SliderField("##pos", "Pos", &pos, 0.0f, 1.0f, width * 0.55f)) { stops[selected].Position = pos; changed = true; }
		ImGui::SameLine();
		const ID nodeID = node.NodeID;
		const int stopIndex = selected;
		if (ImGui::ColorButton("##stopcolor", ImVec4(stops[selected].Color.r, stops[selected].Color.g, stops[selected].Color.b, stops[selected].Color.a),
			ImGuiColorEditFlags_NoTooltip, ImVec2(width * 0.45f - 4.0f, ImGui::GetFrameHeight()))) {
			m_Editor->RequestPopup([nodeID, stopIndex](NodeGraph& g) {
				Node* n = g.FindNode(nodeID);
				if (!n) return false;
				auto s = MaterialGraph::ParseRamp(n->Properties["Stops"]);
				if (stopIndex >= static_cast<int>(s.size())) return false;
				// ParseRamp sorts by position: the popup edits the stop at the same sorted index
				if (ImGui::ColorPicker4("##stop", glm::value_ptr(s[stopIndex].Color), ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview)) {
					n->Properties["Stops"] = MaterialGraph::WriteRamp(s);
					return true;
				}
				return false;
			});
		}

		if (changed) {
			// Keep the selection on the same stop after sorting
			const MaterialGraph::RampStop sel = stops[selected];
			std::stable_sort(stops.begin(), stops.end(), [](auto& a, auto& b) { return a.Position < b.Position; });
			for (int i = 0; i < static_cast<int>(stops.size()); i++)
				if (stops[i].Position == sel.Position && stops[i].Color == sel.Color) { selected = i; break; }
			node.Properties["Stops"] = MaterialGraph::WriteRamp(stops);
			node.Properties["Selected"] = std::to_string(selected);
		}
		return changed;
	}

} // namespace Lunex
