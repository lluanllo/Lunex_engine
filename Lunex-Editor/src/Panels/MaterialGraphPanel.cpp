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
		m_Editor->EnableGroups = true;
		m_NodePreviews = std::make_unique<MaterialGraph::NodePreviewRenderer>(128);
		m_Editor->DrawNodePreview = [this](Node& node, float width) {
			if (!m_ShowNodePreviews) return;
			// Like Blender's node previews: textures, colors, vectors, converters (not shaders / constants)
			const std::string& t = node.TypeName;
			if (t == MaterialGraph::ValueNode || t == MaterialGraph::RGBNode || t == MaterialGraph::TimeNode ||
				t == MaterialGraph::FloatNode || t == MaterialGraph::ColorNode || node.IsGroup() ||
				t == GroupInputType || t == GroupOutputType) return;
			if (const NodeTypeInfo* info = MaterialGraph::GetRegistry().Get(t); info && (info->Category == "Shader" || info->Category == "Output")) return;
			uint32_t tex = 0;
			glm::vec2 uv0, uv1;
			if (!m_NodePreviews->GetTile(node.NodeID, tex, uv0, uv1)) return;
			const float s = std::min(width, 150.0f);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - s) * 0.5f);
			ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(s, s), ImVec2(uv0.x, uv0.y), ImVec2(uv1.x, uv1.y));
		};
		m_Editor->OnPreviewNode = [this](NodeGraph& g, ID node, int output) { return PreviewNode(g, node, output); };
		m_Editor->OnTextureSetup = [this](NodeGraph& g, ID node) { return TextureSetup(g, node); };
		m_Editor->OnPrincipledTextureSetup = [this](NodeGraph& g, ID node) { return PrincipledTextureSetup(g, node); };
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
		if (m_ShowNodePreviews)
			if (auto graph = m_Material->GetNodeGraph()) m_NodePreviews->Update(m_Editor->ResolveCurrentGraph(*graph));
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
			ImGui::TextDisabled("Shift+A Add | Ctrl+G Group, Tab enter/exit | Ctrl+RMB drag Cut links | Shift+RMB drag Reroute | Drop node on link: insert | Ctrl+Shift+Click Preview | Ctrl+T / Ctrl+Shift+T Textures | F Link | Ctrl+X Dissolve | Alt+RMB Lazy connect | Ctrl+H Hide sockets");
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

		// Preview controls (right side, or right after the status when the window is narrow)
		const float right = ImGui::GetWindowContentRegionMax().x;
		const float afterStatus = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + 20.0f;
		ImGui::SameLine(std::max(afterStatus, right - 420.0f));
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
		if (ImGui::Checkbox("Node previews", &m_ShowNodePreviews) && m_ShowNodePreviews) m_NodePreviews->Invalidate();
		ImGui::SameLine();
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
		if (auto graph = m_Material->GetNodeGraph()) DrawDetails(m_Editor->GetCurrentGraph() ? *m_Editor->GetCurrentGraph() : *graph);

		if (!m_Messages.empty()) {
			ImGui::Spacing();
			ImGui::SeparatorText("Messages");
			for (const auto& m : m_Messages) ImGui::TextWrapped("! %s", m.c_str());
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	void MaterialGraphPanel::DrawDetails(NodeGraph& graph) {
		auto root = m_Material->GetNodeGraph();
		auto commit = [&] {
			graph.Touch();
			if (root && root.get() != &graph) { root->SyncGroupsDeep(); root->Touch(); }
			OnGraphChanged();
		};

		// ---- Group interface (while editing a group): sockets of the group node ----
		if (m_Editor->IsInsideGroup()) {
			ImGui::SeparatorText("Group Interface");
			bool ifaceChanged = false;
			static const char* kTypes[] = { "Float", "Vec3", "Color", "Shader" };
			static const PinType kTypeValues[] = { PinType::Float, PinType::Vec3, PinType::Color, PinType::Shader };
			auto socketList = [&](bool inputs) {
				auto& sockets = inputs ? graph.GetInterfaceInputs() : graph.GetInterfaceOutputs();
				ImGui::TextDisabled(inputs ? "Inputs" : "Outputs");
				int remove = -1;
				for (size_t i = 0; i < sockets.size(); i++) {
					InterfaceSocket& sock = sockets[i];
					ImGui::PushID(static_cast<int>(i) + (inputs ? 0 : 1000));
					char buf[64];
					strncpy_s(buf, sock.Name.c_str(), sizeof(buf) - 1);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
					if (ImGui::InputText("##name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0]) {
						// Rename keeps the links: rename the pins of the Group Input / Output node too
						if (Node* io = graph.FindNodeByType(inputs ? GroupInputType : GroupOutputType))
							if (Pin* pin = inputs ? io->FindOutput(sock.Name) : io->FindInput(sock.Name)) pin->Name = buf;
						sock.Name = buf;
						ifaceChanged = true;
					}
					ImGui::SameLine();
					int type = 0;
					for (int t = 0; t < 4; t++) if (kTypeValues[t] == sock.Type) type = t;
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 26.0f);
					if (ImGui::Combo("##type", &type, kTypes, 4)) { sock.Type = kTypeValues[type]; ifaceChanged = true; }
					ImGui::SameLine();
					if (ImGui::SmallButton("x")) remove = static_cast<int>(i);
					if (inputs && sock.Type == PinType::Float)
						ifaceChanged |= UI::NodeGraphEditor::SliderField("##def", "Default", &sock.Default.x, sock.Range.x, sock.Range.y, ImGui::GetContentRegionAvail().x);
					else if (inputs && sock.Type == PinType::Color)
						ifaceChanged |= ImGui::ColorEdit4("Default", glm::value_ptr(sock.Default), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
					ImGui::PopID();
				}
				if (remove >= 0) { graph.RemoveInterfaceSocket(inputs, static_cast<size_t>(remove)); ifaceChanged = true; }
				if (ImGui::SmallButton(inputs ? "+ Input" : "+ Output")) { graph.AddInterfaceSocket(inputs, inputs ? "Input" : "Output", PinType::Float); ifaceChanged = true; }
			};
			socketList(true);
			ImGui::Spacing();
			socketList(false);
			ImGui::TextDisabled("Tip: drag a link onto the empty socket of Group Input/Output.");
			if (ifaceChanged) { graph.SyncInterfaceNodes(); commit(); }
			ImGui::Spacing();
		}

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

		const NodeTypeInfo* info = graph.GetTypeInfo(node->TypeName);
		if (node->IsGroup()) {
			char buf[64];
			strncpy_s(buf, node->Title.c_str(), sizeof(buf) - 1);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::InputText("##groupname", buf, sizeof(buf)) && buf[0]) { node->Title = buf; commit(); }
			ImGui::TextDisabled("Tab: edit the group   Ctrl+Alt+G: ungroup");
		}
		else ImGui::TextUnformatted(node->Title.c_str());
		if (info && !info->Description.empty()) ImGui::TextDisabled("%s", info->Description.c_str());
		ImGui::Spacing();

		bool changed = false;
		const float width = ImGui::GetContentRegionAvail().x;
		ImGui::PushID(static_cast<int>(node->NodeID));
		if (info) {
			for (const auto& e : info->Enums) {
				auto found = node->Properties.find(e.Key);
				const std::string value = (found != node->Properties.end() && !found->second.empty()) ? found->second : (e.Options.empty() ? "" : e.Options[0]);
				ImGui::TextDisabled("%s", e.Key.c_str());
				ImGui::SetNextItemWidth(width);
				if (ImGui::BeginCombo(("##" + e.Key).c_str(), value.c_str())) {
					for (const auto& o : e.Options)
						if (ImGui::Selectable(o.c_str(), o == value)) { node->Properties[e.Key] = o; changed = true; }
					ImGui::EndCombo();
				}
			}
			for (const auto& key : info->Toggles) {
				bool v = node->Properties[key] == "1";
				if (ImGui::Checkbox(key.c_str(), &v)) { node->Properties[key] = v ? "1" : "0"; changed = true; }
			}
			for (const auto& num : info->Numbers) {
				auto it = node->Properties.find(num.Key);
				float v = it != node->Properties.end() ? static_cast<float>(std::atof(it->second.c_str())) : num.Min;
				ImGui::PushID(num.Key.c_str());
				if (UI::NodeGraphEditor::SliderField("##num", num.Key.c_str(), &v, num.Min, num.Max, width, num.IsInt)) {
					char buf[32];
					if (num.IsInt) snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(v)));
					else snprintf(buf, sizeof(buf), "%g", v);
					node->Properties[num.Key] = buf;
					changed = true;
				}
				ImGui::PopID();
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
		if (changed) commit();
	}

	// ============================================================================
	// NODE WRANGLER (material hooks)
	// ============================================================================

	namespace {
		std::string Lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }

		// Texture Coordinate -> Mapping chain feeding `vectorPin`
		void AddMappingChain(NodeGraph& g, ID vectorPin, glm::vec2 at) {
			Node* tc = g.AddNode(MaterialGraph::TexCoordNode, { at.x - 420.0f, at.y });
			const ID tcID = tc->NodeID;
			Node* mapping = g.AddNode(MaterialGraph::MappingNode, { at.x - 220.0f, at.y });
			const ID mapID = mapping->NodeID;
			g.Connect(g.FindNode(tcID)->FindOutput("UV")->PinID, g.FindNode(mapID)->FindInput("Vector")->PinID);
			g.Connect(g.FindNode(mapID)->FindOutput("Vector")->PinID, vectorPin);
		}

		std::vector<std::string> OpenImagesDialog() {
			return FileDialogs::OpenFiles(
				"Images (*.png;*.jpg;*.jpeg;*.tga;*.bmp)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0",
				"Principled texture setup: select the PBR textures");
		}
	}

	bool MaterialGraphPanel::PreviewNode(NodeGraph& g, ID nodeID, int outputIndex) {
		Node* node = g.FindNode(nodeID);
		Node* output = g.FindNodeByType(MaterialGraph::MaterialOutput);
		if (!node || !output || node == output || node->Outputs.empty()) return false;
		if (node->Properties.count("__viewer")) return false;
		const Pin& out = node->Outputs[static_cast<size_t>(outputIndex) % node->Outputs.size()];
		const ID outPin = out.PinID;
		const ID surface = output->FindInput("Surface")->PinID;

		if (out.Type == Graph::PinType::Shader) {
			g.Connect(outPin, surface);
		}
		else {
			// Viewer: an Emission shader showing the value (created once, like Node Wrangler)
			Node* viewer = nullptr;
			for (auto& n : g.GetNodes()) if (n.Properties.count("__viewer")) { viewer = &n; break; }
			if (!viewer) {
				const glm::vec2 at = output->Position + glm::vec2(0.0f, 160.0f);
				viewer = g.AddNode(MaterialGraph::EmissionShader, at);
				viewer->Title = "Viewer";
				viewer->Properties["__viewer"] = "1";
			}
			const ID viewerID = viewer->NodeID;
			g.Connect(outPin, g.FindNode(viewerID)->FindInput("Color")->PinID);
			g.Connect(g.FindNode(viewerID)->FindOutput("Emission")->PinID, g.FindNodeByType(MaterialGraph::MaterialOutput)->FindInput("Surface")->PinID);
		}
		OnGraphChanged();
		return true;
	}

	bool MaterialGraphPanel::TextureSetup(NodeGraph& g, ID nodeID) {
		Node* node = g.FindNode(nodeID);
		if (!node) return false;
		const glm::vec2 at = node->Position;

		// Texture node selected: give it Texture Coordinate + Mapping
		if (Pin* vec = node->FindInput("Vector"); vec && !g.GetLinkInto(vec->PinID)) {
			AddMappingChain(g, vec->PinID, at);
			OnGraphChanged();
			return true;
		}
		// Shader / other node: Image Texture (+ mapping) into its first free color input
		ID target = 0;
		for (const auto& in : node->Inputs)
			if (in.Type == Graph::PinType::Color && !g.GetLinkInto(in.PinID)) { target = in.PinID; break; }
		if (!target) return false;
		Node* img = g.AddNode(MaterialGraph::ImageTexture, { at.x - 300.0f, at.y });
		const ID imgID = img->NodeID;
		g.Connect(g.FindNode(imgID)->FindOutput("Color")->PinID, target);
		AddMappingChain(g, g.FindNode(imgID)->FindInput("Vector")->PinID, g.FindNode(imgID)->Position);
		OnGraphChanged();
		return true;
	}

	bool MaterialGraphPanel::PrincipledTextureSetup(NodeGraph& g, ID nodeID) {
		Node* bsdf = g.FindNode(nodeID);
		if (!bsdf || bsdf->TypeName != MaterialGraph::PrincipledBSDF) return false;
		const std::vector<std::string> files = OpenImagesDialog();
		if (files.empty()) return false;

		// Name keywords -> socket (Node Wrangler conventions)
		struct Rule { const char* Socket; std::vector<const char*> Tags; };
		static const std::vector<Rule> rules = {
			{ "Base Color", { "diffuse", "diff", "albedo", "base", "basecolor", "col", "color", "colour" } },
			{ "Normal", { "normal", "nor", "nrm", "nrml", "norm" } },
			{ "Roughness", { "roughness", "rough", "rgh" } },
			{ "Metallic", { "metallic", "metalness", "metal", "mtl" } },
			{ "Ambient Occlusion", { "ao", "ambient", "occlusion" } },
			{ "Specular", { "specularity", "specular", "spec", "spc" } },
			{ "Emission Color", { "emission", "emissive", "emit" } },
			{ "Alpha", { "alpha", "opacity" } },
			{ "Height", { "displacement", "displace", "disp", "dsp", "height", "heightmap", "bump" } },
		};
		auto classify = [&](const std::string& path) -> const char* {
			std::string stem = Lower(std::filesystem::path(path).stem().string());
			for (char& c : stem) if (c == '-' || c == '.' || c == ' ') c = '_';
			std::vector<std::string> tokens;
			size_t start = 0;
			for (size_t i = 0; i <= stem.size(); i++)
				if (i == stem.size() || stem[i] == '_') { if (i > start) tokens.push_back(stem.substr(start, i - start)); start = i + 1; }
			for (const auto& rule : rules)
				for (const auto& t : tokens)
					for (const char* tag : rule.Tags) if (t == tag) return rule.Socket;
			return nullptr;
		};

		const glm::vec2 base = bsdf->Position;
		float y = base.y;
		std::vector<ID> vectorPins;
		for (const auto& file : files) {
			const char* socket = classify(file);
			if (!socket) { LNX_LOG_WARN("Principled setup: could not classify {0}", file); continue; }
			Node* img = g.AddNode(MaterialGraph::ImageTexture, { base.x - 520.0f, y });
			img->Properties["Path"] = file;
			// Node Wrangler: every map lands in its correct color space (only Base Color / Emission stay sRGB)
			img->Properties["Color Space"] = (std::string(socket) == "Base Color" || std::string(socket) == "Emission Color") ? "sRGB" : "Non-Color";
			const ID imgID = img->NodeID;
			vectorPins.push_back(img->FindInput("Vector")->PinID);
			const ID color = g.FindNode(imgID)->FindOutput("Color")->PinID;
			const ID bsdfID = nodeID;   // AddNode may reallocate: never keep Node* across it
			const std::string s = socket;
			if (s == "Normal") {
				Node* nm = g.AddNode(MaterialGraph::NormalMapNode, { base.x - 220.0f, y });
				const ID nmID = nm->NodeID;
				g.Connect(color, g.FindNode(nmID)->FindInput("Color")->PinID);
				g.Connect(g.FindNode(nmID)->FindOutput("Normal")->PinID, g.FindNode(bsdfID)->FindInput("Normal")->PinID);
			}
			else if (s == "Height") {
				Node* bump = g.AddNode(MaterialGraph::BumpNode, { base.x - 220.0f, y });
				const ID bumpID = bump->NodeID;
				g.Connect(color, g.FindNode(bumpID)->FindInput("Height")->PinID);
				if (!g.GetLinkInto(g.FindNode(bsdfID)->FindInput("Normal")->PinID))
					g.Connect(g.FindNode(bumpID)->FindOutput("Normal")->PinID, g.FindNode(bsdfID)->FindInput("Normal")->PinID);
			}
			else if (s == "Alpha") {
				g.Connect(g.FindNode(imgID)->FindOutput("Alpha")->PinID, g.FindNode(bsdfID)->FindInput("Alpha")->PinID);
			}
			else if (Pin* in = g.FindNode(bsdfID)->FindInput(socket)) {
				g.Connect(color, in->PinID);
				if (s == "Emission Color") g.FindNode(bsdfID)->FindInput("Emission Strength")->DefaultValue = glm::vec4(1.0f);
			}
			bsdf = g.FindNode(bsdfID);
			y += 260.0f;
		}
		if (vectorPins.empty()) return false;

		// One shared Texture Coordinate + Mapping for every texture
		Node* tc = g.AddNode(MaterialGraph::TexCoordNode, { base.x - 960.0f, base.y });
		const ID tcID = tc->NodeID;
		Node* mapping = g.AddNode(MaterialGraph::MappingNode, { base.x - 760.0f, base.y });
		const ID mapID = mapping->NodeID;
		g.Connect(g.FindNode(tcID)->FindOutput("UV")->PinID, g.FindNode(mapID)->FindInput("Vector")->PinID);
		for (ID v : vectorPins) g.Connect(g.FindNode(mapID)->FindOutput("Vector")->PinID, v);
		OnGraphChanged();
		return true;
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
		if (t == MaterialGraph::ImageTexture || t == MaterialGraph::TextureNode || t == MaterialGraph::EnvironmentTexture) return DrawImageBody(node);
		if (t == MaterialGraph::ColorRampNode) return DrawRampBody(node);
		if (t == MaterialGraph::RGBCurvesNode || t == MaterialGraph::VectorCurvesNode || t == MaterialGraph::FloatCurveNode) return DrawCurveBody(node);
		if (t == MaterialGraph::NormalNode) return DrawNormalBody(node);
		if (t == MaterialGraph::ValueNode || t == MaterialGraph::FloatNode)
			return UI::NodeGraphEditor::SliderField("##value", "", &node.Value.x, 0.0f, 0.0f, width);
		if (t == MaterialGraph::RGBNode || t == MaterialGraph::ColorNode) {
			return m_Editor->ColorField(node.NodeID, 0, node.Value, width, 46.0f);
		}
		return false;
	}

	// Blender curve widget: channel buttons, click to add, drag to move, double-click to delete
	bool MaterialGraphPanel::DrawCurveBody(Node& node) {
		bool changed = false;
		const float width = ImGui::CalcItemWidth();
		const std::string& t = node.TypeName;
		std::vector<std::string> channels;
		float lo = 0.0f, hi = 1.0f;
		if (t == MaterialGraph::RGBCurvesNode) channels = { "C", "R", "G", "B" };
		else if (t == MaterialGraph::VectorCurvesNode) { channels = { "X", "Y", "Z" }; lo = -1.0f; hi = 1.0f; }
		else channels = { "V" };

		std::string& channel = node.Properties["Channel"];
		if (std::find(channels.begin(), channels.end(), channel) == channels.end()) channel = channels[0];
		if (channels.size() > 1) {
			for (size_t i = 0; i < channels.size(); i++) {
				if (i) ImGui::SameLine(0.0f, 2.0f);
				const bool active = channel == channels[i];
				if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.45f, 0.70f, 1.0f));
				if (ImGui::Button(channels[i].c_str(), ImVec2(28.0f, 0.0f))) channel = channels[i];
				if (active) ImGui::PopStyleColor();
			}
		}

		const std::string key = "Curve " + channel;
		auto points = MaterialGraph::ParseCurve(node.Properties[key]);
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 size(width, width * 0.8f);
		const ImVec2 p1(p0.x + size.x, p0.y + size.y);
		auto toScreen = [&](float x, float y) { return ImVec2(p0.x + (x - lo) / (hi - lo) * size.x, p1.y - (y - lo) / (hi - lo) * size.y); };
		auto toCurve = [&](ImVec2 s) {
			return glm::vec2(std::clamp(lo + (s.x - p0.x) / size.x * (hi - lo), lo, hi), std::clamp(lo + (p1.y - s.y) / size.y * (hi - lo), lo, hi));
		};

		ImGui::InvisibleButton("##curve", size);
		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p0, p1, IM_COL32(32, 32, 32, 255), 3.0f);
		for (int i = 1; i < 4; i++) {
			const float fx = p0.x + size.x * i / 4.0f, fy = p0.y + size.y * i / 4.0f;
			dl->AddLine(ImVec2(fx, p0.y), ImVec2(fx, p1.y), IM_COL32(60, 60, 60, 255));
			dl->AddLine(ImVec2(p0.x, fy), ImVec2(p1.x, fy), IM_COL32(60, 60, 60, 255));
		}
		const ImU32 col = channel == "R" || channel == "X" ? IM_COL32(230, 80, 80, 255) : channel == "G" || channel == "Y" ? IM_COL32(110, 210, 90, 255)
			: channel == "B" || channel == "Z" ? IM_COL32(90, 140, 240, 255) : IM_COL32(220, 220, 220, 255);
		ImVec2 prev;
		for (int i = 0; i <= 64; i++) {
			const float x = lo + (hi - lo) * i / 64.0f;
			const ImVec2 sp = toScreen(x, std::clamp(MaterialGraph::EvaluateCurve(points, x), lo, hi));
			if (i) dl->AddLine(prev, sp, col, 2.0f);
			prev = sp;
		}

		// Interaction (state per node: which point is being dragged)
		static Graph::ID s_DragNode = 0;
		static int s_DragPoint = -1;
		const ImVec2 mouse = ImGui::GetMousePos();
		int hot = -1;
		for (int i = 0; i < static_cast<int>(points.size()); i++) {
			const ImVec2 sp = toScreen(points[i].X, points[i].Y);
			if ((mouse.x - sp.x) * (mouse.x - sp.x) + (mouse.y - sp.y) * (mouse.y - sp.y) < 49.0f) hot = i;
		}
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hot >= 0 && points.size() > 2) {
			points.erase(points.begin() + hot);
			changed = true;
			s_DragPoint = -1;
		}
		else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (hot < 0) {
				const glm::vec2 c = toCurve(mouse);
				points.push_back({ c.x, c.y });
				std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.X < b.X; });
				for (int i = 0; i < static_cast<int>(points.size()); i++) if (points[i].X == c.x && points[i].Y == c.y) hot = i;
				changed = true;
			}
			s_DragNode = node.NodeID;
			s_DragPoint = hot;
		}
		if (s_DragNode == node.NodeID && s_DragPoint >= 0 && s_DragPoint < static_cast<int>(points.size())) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				const glm::vec2 c = toCurve(mouse);
				const float minX = s_DragPoint > 0 ? points[s_DragPoint - 1].X + 0.001f : lo;
				const float maxX = s_DragPoint + 1 < static_cast<int>(points.size()) ? points[s_DragPoint + 1].X - 0.001f : hi;
				const float nx = std::clamp(c.x, minX, maxX);
				if (nx != points[s_DragPoint].X || c.y != points[s_DragPoint].Y) { points[s_DragPoint] = { nx, c.y }; changed = true; }
			}
			else s_DragPoint = -1;
		}
		for (int i = 0; i < static_cast<int>(points.size()); i++) {
			const ImVec2 sp = toScreen(points[i].X, points[i].Y);
			const bool sel = s_DragNode == node.NodeID && s_DragPoint == i;
			dl->AddCircleFilled(sp, sel || i == hot ? 5.0f : 4.0f, sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255));
			dl->AddCircle(sp, sel || i == hot ? 5.0f : 4.0f, IM_COL32(230, 230, 230, 255), 0, 1.5f);
		}
		if (changed) node.Properties[key] = MaterialGraph::WriteCurve(points);
		return changed;
	}

	bool MaterialGraphPanel::DrawNormalBody(Node& node) {
		// Direction of the Normal node (Blender draws a sphere widget; here X/Y/Z)
		bool changed = false;
		const float width = ImGui::CalcItemWidth();
		static const char* axes[] = { "X", "Y", "Z" };
		for (int i = 0; i < 3; i++) {
			ImGui::PushID(i);
			changed |= UI::NodeGraphEditor::SliderField("##dir", axes[i], &node.Value[i], -1.0f, 1.0f, width);
			ImGui::PopID();
		}
		return changed;
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

		// New image: data maps (normal, roughness, ORM...) default to Non-Color, colors to sRGB
		if (changed && !path.empty()) node.Properties["Color Space"] = MaterialGraph::GuessColorSpace(path);
		else if (!path.empty() && !node.Properties.count("Color Space")) node.Properties["Color Space"] = MaterialGraph::GuessColorSpace(path);

		if (auto proj = node.Properties.find("Projection"); proj != node.Properties.end() && proj->second == "Box")
			changed |= UI::NodeGraphEditor::SliderField("##blend", "Blend", &node.Value.x, 0.0f, 1.0f, width);
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
