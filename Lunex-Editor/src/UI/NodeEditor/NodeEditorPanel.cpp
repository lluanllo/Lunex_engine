/**
 * @file NodeEditorPanel.cpp
 * @brief Implementation of the visual node editor panel
 */

#include "NodeEditorPanel.h"
#include "NodeEditorStyle.h"

#include "../LunexUI.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>

#include <algorithm>
#include <cmath>

namespace Lunex {

	// ============================================================================
	// CONSTRUCTION / DESTRUCTION
	// ============================================================================

	NodeEditorPanel::NodeEditorPanel() {
		m_EditorContext = ImNodes::EditorContextCreate();
	}

	NodeEditorPanel::~NodeEditorPanel() {
		if (m_EditorContext) {
			ImNodes::EditorContextFree(m_EditorContext);
			m_EditorContext = nullptr;
		}
	}

	// ============================================================================
	// GRAPH MANAGEMENT
	// ============================================================================

	void NodeEditorPanel::SetGraph(Ref<NodeGraph::NodeGraph> graph) {
		m_Graph = graph;
		m_SelectedNodeID = NodeGraph::InvalidNodeID;
		m_InitializedNodePositions.clear();
		
		// Reset editor context for new graph
		if (m_EditorContext) {
			ImNodes::EditorContextFree(m_EditorContext);
		}
		m_EditorContext = ImNodes::EditorContextCreate();
	}

	void NodeEditorPanel::SetDelegate(Ref<NodeEditorDelegate> delegate) {
		m_Delegate = delegate;
	}

	// ============================================================================
	// UPDATE
	// ============================================================================

	void NodeEditorPanel::OnUpdate(float deltaTime) {
		// Future: animation, auto-layout, etc.
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void NodeEditorPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		std::string windowTitle = m_Title;
		if (m_Graph && m_Graph->IsDirty())
			windowTitle += " *";
		windowTitle += "###NodeEditor";

		ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

		if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
			DrawMenuBar();
			DrawToolbar();
			
			if (m_Graph) {
				// Main content: node editor + optional properties
				float propertiesWidth = m_ShowProperties ? 280.0f : 0.0f;
				float editorWidth = ImGui::GetContentRegionAvail().x - propertiesWidth;

				if (m_ShowProperties && propertiesWidth > 0) {
					ImGui::BeginChild("##NodeEditorCanvas", ImVec2(editorWidth, 0), false);
				}

				DrawNodeEditor();

				if (m_ShowProperties && propertiesWidth > 0) {
					ImGui::EndChild();
					ImGui::SameLine();
					ImGui::BeginChild("##NodeProperties", ImVec2(0, 0), true);
					DrawPropertiesPanel();
					ImGui::EndChild();
				}
			}
			else {
				// No graph loaded
				ImVec2 center = ImGui::GetContentRegionAvail();
				ImVec2 textSize = ImGui::CalcTextSize("No graph loaded");
				ImGui::SetCursorPos(ImVec2(
					(center.x - textSize.x) * 0.5f,
					(center.y - textSize.y) * 0.5f));
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No graph loaded");
			}
		}
		ImGui::End();
	}

	void NodeEditorPanel::OnImGuiRenderEmbedded() {
		if (!m_IsOpen) return;

		DrawToolbar();

		if (m_Graph) {
			float propertiesWidth = m_ShowProperties ? 280.0f : 0.0f;
			float editorWidth = ImGui::GetContentRegionAvail().x - propertiesWidth;

			if (m_ShowProperties && propertiesWidth > 0) {
				ImGui::BeginChild("##NodeEditorCanvasEmbed", ImVec2(editorWidth, 0), false);
			}

			DrawNodeEditor();

			if (m_ShowProperties && propertiesWidth > 0) {
				ImGui::EndChild();
				ImGui::SameLine();
				ImGui::BeginChild("##NodePropertiesEmbed", ImVec2(0, 0), true);
				DrawPropertiesPanel();
				ImGui::EndChild();
			}
		}
		else {
			ImVec2 center = ImGui::GetContentRegionAvail();
			ImVec2 textSize = ImGui::CalcTextSize("No graph loaded");
			ImGui::SetCursorPos(ImVec2(
				(center.x - textSize.x) * 0.5f,
				(center.y - textSize.y) * 0.5f));
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No graph loaded");
		}
	}

	// ============================================================================
	// MENU BAR
	// ============================================================================

	void NodeEditorPanel::DrawMenuBar() {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Graph")) {
				if (ImGui::MenuItem("Clear All", nullptr, false, m_Graph != nullptr)) {
					if (m_Graph) {
						m_Graph->Clear();
						NotifyGraphChanged();
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Validate", nullptr, false, m_Graph != nullptr)) {
					if (m_Graph) m_Graph->Validate();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Mini Map", nullptr, &m_ShowMiniMap);
				ImGui::MenuItem("Properties", nullptr, &m_ShowProperties);
				ImGui::MenuItem("Grid", nullptr, &m_ShowGrid);
				ImGui::MenuItem("Node Previews", nullptr, &m_ShowNodePreviews);
				ImGui::Separator();
				if (ImGui::MenuItem("Fit to Screen")) {
					// TODO: implement fit-to-screen
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}

	// ============================================================================
	// TOOLBAR
	// ============================================================================

	void NodeEditorPanel::DrawToolbar() {
		if (!m_Graph) return;

		// Domain info
		ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), "%s",
			NodeGraph::GraphDomainToString(m_Graph->GetDomain()));
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), " | Nodes: %d | Links: %d",
			(int)m_Graph->GetNodeCount(), (int)m_Graph->GetLinkCount());

		// Domain-specific toolbar
		if (m_Delegate) {
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			m_Delegate->DrawToolbar(*m_Graph);
		}

		ImGui::Separator();
	}

	// ============================================================================
	// NODE EDITOR
	// ============================================================================

	void NodeEditorPanel::DrawNodeEditor() {
		if (!m_Graph) return;

		ImNodes::EditorContextSet(m_EditorContext);

		// Apply Lunex style
		UI::ApplyNodeEditorStyle();

		// Configure imnodes IO: use middle mouse for panning, prevent right-click panning
		// This avoids conflicts with Ctrl+RMB link cutting and right-click context menu
		ImNodesIO& io = ImNodes::GetIO();
		io.EmulateThreeButtonMouse.Modifier = &ImGui::GetIO().KeyAlt;
		io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
		io.AltMouseButton = ImGuiMouseButton_Middle;

		ImNodes::BeginNodeEditor();

		// Render all nodes
		for (auto& [id, node] : m_Graph->GetNodes()) {
			RenderNode(*node);
		}

		// Render all links
		RenderLinks();

		// Mini map
		if (m_ShowMiniMap) {
			ImNodes::MiniMap(0.15f, ImNodesMiniMapLocation_BottomRight);
		}

		// Capture state inside BeginNodeEditor/EndNodeEditor scope
		bool editorHovered = ImNodes::IsEditorHovered();

		// Left-click on empty area to open add node menu
		bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && editorHovered;

		ImVec2 clickMousePos = ImGui::GetMousePos();

		// Shift+A to open add node menu (like Blender), even while navigating
		bool shiftA = ImGui::IsKeyPressed(ImGuiKey_A) && ImGui::GetIO().KeyShift && editorHovered && !ImGui::IsAnyItemActive();

		// Draw link cut line overlay inside the editor
		if (m_IsCuttingLinks) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddLine(m_CutLineStart, m_CutLineEnd,
				IM_COL32(255, 50, 50, 200), 2.0f);
		}

		ImNodes::EndNodeEditor();

		// Handle interactions AFTER EndNodeEditor (required by imnodes API)
		HandleLinkCreation();
		HandleLinkDeletion();
		HandleNodeDeletion();
		HandleLinkCutting();
		HandleCtrlClickConnect();

		// Handle left-click context menu AFTER EndNodeEditor
		if (leftClicked && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
			int hoveredNode = -1;
			int hoveredLink = -1;
			int hoveredPin = -1;

			if (!ImNodes::IsNodeHovered(&hoveredNode) &&
				!ImNodes::IsLinkHovered(&hoveredLink) &&
				!ImNodes::IsPinHovered(&hoveredPin)) {
				m_OpenCreatePopup = true;
				m_CreateNodePosition = { clickMousePos.x, clickMousePos.y };
			}
		}

		// Shift+A to add node (like Blender)
		if (shiftA) {
			m_OpenCreatePopup = true;
			ImVec2 mousePos = ImGui::GetMousePos();
			m_CreateNodePosition = { mousePos.x, mousePos.y };
		}

		if (m_OpenCreatePopup) {
			ImGui::OpenPopup("##CreateNodePopup");
			m_OpenCreatePopup = false;
			m_SearchFilter.clear();
		}

		// Update selection
		int numSelected = ImNodes::NumSelectedNodes();
		if (numSelected == 1) {
			int selectedId;
			ImNodes::GetSelectedNodes(&selectedId);
			m_SelectedNodeID = selectedId;
		}
		else if (numSelected == 0) {
			m_SelectedNodeID = NodeGraph::InvalidNodeID;
		}

		// Node creation popup
		DrawNodeCreationPopup();
	}

	// ============================================================================
	// NODE RENDERING
	// ============================================================================

	void NodeEditorPanel::RenderNode(NodeGraph::Node& node) {
		// Only set node position on first render - after that imnodes manages it
		if (m_InitializedNodePositions.find(node.ID) == m_InitializedNodePositions.end()) {
			ImNodes::SetNodeGridSpacePos(node.ID, ImVec2(node.Position.x, node.Position.y));
			m_InitializedNodePositions.insert(node.ID);
		}

		// Node header color based on node type
		ImNodes::PushColorStyle(ImNodesCol_TitleBar, node.HeaderColor);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
			NodeGraph::PackColor(
				std::min(255, (int)((node.HeaderColor & 0xFF)) + 20),
				std::min(255, (int)(((node.HeaderColor >> 8) & 0xFF)) + 20),
				std::min(255, (int)(((node.HeaderColor >> 16) & 0xFF)) + 20)));
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
			UI::Colors::Primary().ToImU32());

		// Status outline
		if (node.Status == NodeGraph::NodeStatus::Error) {
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, UI::Colors::Danger().ToImU32());
		}
		else if (node.Status == NodeGraph::NodeStatus::Warning) {
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, UI::Colors::Warning().ToImU32());
		}

		ImNodes::BeginNode(node.ID);

		// Node preview thumbnail above the title bar (like Blender's Node Preview addon)
		if (m_ShowNodePreviews && m_Delegate) {
			m_Delegate->DrawNodePreview(node, *m_Graph);
		}

		// Title bar
		ImNodes::BeginNodeTitleBar();
		ImGui::TextUnformatted(node.DisplayName.c_str());
		ImNodes::EndNodeTitleBar();

		// Input pins
		for (auto& pin : node.Inputs) {
			if (pin.IsHidden) continue;
			RenderPin(pin, node);
		}

		// Custom body (delegate)
		if (m_Delegate) {
			m_Delegate->DrawNodeBody(node, *m_Graph);
		}

		// Output pins
		for (auto& pin : node.Outputs) {
			if (pin.IsHidden) continue;
			RenderPin(pin, node);
		}

		// Status message
		if (node.Status != NodeGraph::NodeStatus::None && !node.StatusMessage.empty()) {
			ImGui::Spacing();
			ImVec4 statusColor = node.Status == NodeGraph::NodeStatus::Error
				? UI::Colors::Danger().ToImVec4()
				: (node.Status == NodeGraph::NodeStatus::Warning
					? UI::Colors::Warning().ToImVec4()
					: UI::Colors::Success().ToImVec4());
			ImGui::TextColored(statusColor, "%s", node.StatusMessage.c_str());
		}

		ImNodes::EndNode();

		// Update node position from imnodes
		ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.ID);
		node.Position = { pos.x, pos.y };

		// Pop status outline
		if (node.Status == NodeGraph::NodeStatus::Error || node.Status == NodeGraph::NodeStatus::Warning) {
			ImNodes::PopColorStyle();
		}

		ImNodes::PopColorStyle(); // TitleBarSelected
		ImNodes::PopColorStyle(); // TitleBarHovered
		ImNodes::PopColorStyle(); // TitleBar
	}

	void NodeEditorPanel::RenderPin(NodeGraph::Pin& pin, NodeGraph::Node& node) {
		uint32_t pinColor = NodeGraph::Pin::GetTypeColor(pin.DataType);
		ImNodes::PushColorStyle(ImNodesCol_Pin, pinColor);
		ImNodes::PushColorStyle(ImNodesCol_PinHovered,
			NodeGraph::PackColor(
				std::min(255, (int)((pinColor & 0xFF)) + 40),
				std::min(255, (int)(((pinColor >> 8) & 0xFF)) + 40),
				std::min(255, (int)(((pinColor >> 16) & 0xFF)) + 40)));

		// Determine pin shape
		ImNodesPinShape shape = ImNodesPinShape_CircleFilled;
		if (pin.DataType == NodeGraph::PinDataType::Flow) {
			shape = ImNodesPinShape_TriangleFilled;
		}
		else if (!pin.IsConnected) {
			shape = ImNodesPinShape_Circle;
		}

		if (pin.IsInput()) {
			ImNodes::BeginInputAttribute(pin.ID, shape);
			ImGui::TextUnformatted(pin.Name.c_str());

			// Show inline editor for unconnected inputs
			if (!pin.IsConnected) {
				ImGui::SameLine();
				
				// Try delegate first
				bool handled = false;
				if (m_Delegate) {
					handled = m_Delegate->DrawPinValueEditor(pin);
				}

				if (!handled) {
					RenderPinDefaultValue(pin);
				}
			}

			ImNodes::EndInputAttribute();
		}
		else {
			ImNodes::BeginOutputAttribute(pin.ID, shape);
			
			// Right-align output pin names
			float textWidth = ImGui::CalcTextSize(pin.Name.c_str()).x;
			ImGui::Indent(120.0f - textWidth);
			ImGui::TextUnformatted(pin.Name.c_str());

			ImNodes::EndOutputAttribute();
		}

		ImNodes::PopColorStyle(); // PinHovered
		ImNodes::PopColorStyle(); // Pin
	}

	void NodeEditorPanel::RenderPinDefaultValue(NodeGraph::Pin& pin) {
		ImGui::PushItemWidth(60.0f);
		std::string id = "##pin_" + std::to_string(pin.ID);

		bool changed = false;

		switch (pin.DataType) {
			case NodeGraph::PinDataType::Float: {
				float val = NodeGraph::GetPinValue<float>(pin.DefaultValue, 0.0f);
				if (ImGui::DragFloat(id.c_str(), &val, 0.01f)) {
					pin.DefaultValue = val;
					changed = true;
				}
				break;
			}
			case NodeGraph::PinDataType::Int: {
				int32_t val = NodeGraph::GetPinValue<int32_t>(pin.DefaultValue, 0);
				if (ImGui::DragInt(id.c_str(), &val, 1)) {
					pin.DefaultValue = val;
					changed = true;
				}
				break;
			}
			case NodeGraph::PinDataType::Bool: {
				bool val = NodeGraph::GetPinValue<bool>(pin.DefaultValue, false);
				if (ImGui::Checkbox(id.c_str(), &val)) {
					pin.DefaultValue = val;
					changed = true;
				}
				break;
			}
			case NodeGraph::PinDataType::Vec2: {
				glm::vec2 val = NodeGraph::GetPinValue<glm::vec2>(pin.DefaultValue, glm::vec2(0.0f));
				ImGui::PushItemWidth(120.0f);
				if (ImGui::DragFloat2(id.c_str(), &val.x, 0.01f)) {
					pin.DefaultValue = val;
					changed = true;
				}
				ImGui::PopItemWidth();
				break;
			}
			case NodeGraph::PinDataType::Vec3: {
				glm::vec3 val = NodeGraph::GetPinValue<glm::vec3>(pin.DefaultValue, glm::vec3(0.0f));
				ImGui::PushItemWidth(180.0f);
				if (ImGui::DragFloat3(id.c_str(), &val.x, 0.01f)) {
					pin.DefaultValue = val;
					changed = true;
				}
				ImGui::PopItemWidth();
				break;
			}
			case NodeGraph::PinDataType::Color3: {
				glm::vec3 val = NodeGraph::GetPinValue<glm::vec3>(pin.DefaultValue, glm::vec3(1.0f));
				if (ImGui::ColorEdit3(id.c_str(), &val.x, ImGuiColorEditFlags_NoInputs)) {
					pin.DefaultValue = val;
					changed = true;
				}
				break;
			}
			case NodeGraph::PinDataType::Color4: {
				glm::vec4 val = NodeGraph::GetPinValue<glm::vec4>(pin.DefaultValue, glm::vec4(1.0f));
				if (ImGui::ColorEdit4(id.c_str(), &val.x, ImGuiColorEditFlags_NoInputs)) {
					pin.DefaultValue = val;
					changed = true;
				}
				break;
			}
			default:
				break;
		}

		ImGui::PopItemWidth();

		if (changed) {
			NotifyGraphChanged();
		}
	}

	// ============================================================================
	// LINK RENDERING
	// ============================================================================

	void NodeEditorPanel::RenderLinks() {
		if (!m_Graph) return;

		for (const auto& [id, link] : m_Graph->GetLinks()) {
			ImNodes::PushColorStyle(ImNodesCol_Link, link.Color);
			ImNodes::Link(link.ID, link.StartPinID, link.EndPinID);
			ImNodes::PopColorStyle();
		}
	}

	// ============================================================================
	// INTERACTION HANDLING
	// ============================================================================

	void NodeEditorPanel::HandleLinkCreation() {
		int startAttr, endAttr;
		if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
			if (m_Graph) {
				NodeGraph::LinkID linkID = m_Graph->AddLink(startAttr, endAttr);
				if (linkID != NodeGraph::InvalidLinkID) {
					NotifyGraphChanged();
				}
			}
		}
	}

	void NodeEditorPanel::HandleLinkDeletion() {
		int linkId;
		if (ImNodes::IsLinkDestroyed(&linkId)) {
			if (m_Graph) {
				m_Graph->RemoveLink(linkId);
				NotifyGraphChanged();
			}
		}
	}

	void NodeEditorPanel::HandleNodeDeletion() {
		if (!m_Graph) return;

		// Delete selected nodes/links on Delete key
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::IsAnyItemActive()) {
			int numSelectedLinks = ImNodes::NumSelectedLinks();
			if (numSelectedLinks > 0) {
				std::vector<int> selectedLinks(numSelectedLinks);
				ImNodes::GetSelectedLinks(selectedLinks.data());
				for (int linkId : selectedLinks) {
					m_Graph->RemoveLink(linkId);
				}
				ImNodes::ClearLinkSelection();
			}

			int numSelectedNodes = ImNodes::NumSelectedNodes();
			if (numSelectedNodes > 0) {
				std::vector<int> selectedNodes(numSelectedNodes);
				ImNodes::GetSelectedNodes(selectedNodes.data());
				for (int nodeId : selectedNodes) {
					auto* node = m_Graph->GetNode(nodeId);
					if (node && !NodeGraph::HasFlag(node->Flags, NodeGraph::NodeFlags::NoDelete)) {
						m_Graph->RemoveNode(nodeId);
					}
				}
				ImNodes::ClearNodeSelection();
				m_SelectedNodeID = NodeGraph::InvalidNodeID;
			}

			NotifyGraphChanged();
		}
	}

	// ============================================================================
	// LINK CUTTING (Ctrl+RMB drag, like Blender)
	// ============================================================================

	void NodeEditorPanel::HandleLinkCutting() {
		if (!m_Graph) return;

		bool ctrlHeld = ImGui::GetIO().KeyCtrl;

		// Start cutting on Ctrl+RMB press
		if (ctrlHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			m_IsCuttingLinks = true;
			m_CutLineStart = ImGui::GetMousePos();
			m_CutLineEnd = m_CutLineStart;
			// Save the panning position so we can restore it each frame while cutting
			m_CutStartPanning = ImNodes::EditorContextGetPanning();
		}

		// Update cut line while dragging
		if (m_IsCuttingLinks) {
			m_CutLineEnd = ImGui::GetMousePos();

			// Prevent imnodes from panning while we're cutting by restoring the panning
			ImNodes::EditorContextResetPanning(m_CutStartPanning);

			// On release, find and cut all links that intersect the line
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				m_IsCuttingLinks = false;

				// Get editor panning and window position to convert grid space to screen space
				ImVec2 editorPanning = ImNodes::EditorContextGetPanning();
				ImVec2 editorOrigin = ImGui::GetWindowPos();

				// Check each link for intersection with the cut line
				std::vector<NodeGraph::LinkID> linksToRemove;

				for (const auto& [id, link] : m_Graph->GetLinks()) {
					NodeGraph::Node* startOwner = m_Graph->FindPinOwner(link.StartPinID);
					NodeGraph::Node* endOwner = m_Graph->FindPinOwner(link.EndPinID);
					if (!startOwner || !endOwner) continue;

					// Convert node grid positions to approximate screen positions
					ImVec2 startGridPos = ImNodes::GetNodeGridSpacePos(startOwner->ID);
					ImVec2 endGridPos = ImNodes::GetNodeGridSpacePos(endOwner->ID);

					ImVec2 startScreenPos = ImVec2(
						startGridPos.x + editorPanning.x + editorOrigin.x,
						startGridPos.y + editorPanning.y + editorOrigin.y
					);
					ImVec2 endScreenPos = ImVec2(
						endGridPos.x + editorPanning.x + editorOrigin.x,
						endGridPos.y + editorPanning.y + editorOrigin.y
					);

					// Approximate link endpoints (output is on right side, input on left)
					ImVec2 linkStart = ImVec2(startScreenPos.x + 140.0f, startScreenPos.y + 30.0f);
					ImVec2 linkEnd = ImVec2(endScreenPos.x, endScreenPos.y + 30.0f);

					// Test cut line against multiple segments of a bezier approximation
					auto cross2D = [](ImVec2 a, ImVec2 b) { return a.x * b.y - a.y * b.x; };
					ImVec2 d1 = ImVec2(m_CutLineEnd.x - m_CutLineStart.x, m_CutLineEnd.y - m_CutLineStart.y);

					bool cut = false;

					// Test straight line
					{
						ImVec2 d2 = ImVec2(linkEnd.x - linkStart.x, linkEnd.y - linkStart.y);
						float denom = cross2D(d1, d2);
						if (std::abs(denom) > 0.001f) {
							ImVec2 d3 = ImVec2(linkStart.x - m_CutLineStart.x, linkStart.y - m_CutLineStart.y);
							float t = cross2D(d3, d2) / denom;
							float u = cross2D(d3, d1) / denom;
							if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
								cut = true;
							}
						}
					}

					// Also test bezier segments for curved links
					if (!cut) {
						float midX = (linkStart.x + linkEnd.x) * 0.5f;
						for (int seg = 0; seg < 8; seg++) {
							float ta = (float)seg / 8.0f;
							float tb = (float)(seg + 1) / 8.0f;

							auto bezierPoint = [&](float tt) -> ImVec2 {
								float u0 = 1.0f - tt;
								return ImVec2(
									u0 * u0 * linkStart.x + 2.0f * u0 * tt * midX + tt * tt * linkEnd.x,
									u0 * u0 * linkStart.y + 2.0f * u0 * tt * (linkStart.y + linkEnd.y) * 0.5f + tt * tt * linkEnd.y
								);
							};

							ImVec2 pa = bezierPoint(ta);
							ImVec2 pb = bezierPoint(tb);

							ImVec2 sd = ImVec2(pb.x - pa.x, pb.y - pa.y);
							float sd_denom = cross2D(d1, sd);
							if (std::abs(sd_denom) < 0.001f) continue;

							ImVec2 sd3 = ImVec2(pa.x - m_CutLineStart.x, pa.y - m_CutLineStart.y);
							float st = cross2D(sd3, sd) / sd_denom;
							float su = cross2D(sd3, d1) / sd_denom;

							if (st >= 0.0f && st <= 1.0f && su >= 0.0f && su <= 1.0f) {
								cut = true;
								break;
							}
						}
					}

					if (cut) {
						linksToRemove.push_back(id);
					}
				}

				// Remove duplicates and cut
				std::sort(linksToRemove.begin(), linksToRemove.end());
				linksToRemove.erase(std::unique(linksToRemove.begin(), linksToRemove.end()), linksToRemove.end());

				if (!linksToRemove.empty()) {
					for (auto linkId : linksToRemove) {
						m_Graph->RemoveLink(linkId);
					}
					NotifyGraphChanged();
				}
			}
		}
	}

	// ============================================================================
	// CTRL+CLICK TO AUTO-CONNECT TO OUTPUT
	// ============================================================================

	void NodeEditorPanel::HandleCtrlClickConnect() {
		if (!m_Graph) return;

		if (ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
			int hoveredNode = -1;
			if (ImNodes::IsNodeHovered(&hoveredNode)) {
				AutoConnectToOutput(hoveredNode);
			}
		}
	}

	void NodeEditorPanel::AutoConnectToOutput(NodeGraph::NodeID nodeID) {
		if (!m_Graph) return;

		auto* node = m_Graph->GetNode(nodeID);
		if (!node || node->Outputs.empty()) return;

		// Find the Material Output node (or any output node)
		NodeGraph::Node* outputNode = nullptr;
		for (auto& [id, n] : m_Graph->GetNodes()) {
			if (NodeGraph::HasFlag(n->Flags, NodeGraph::NodeFlags::IsOutput)) {
				outputNode = n.get();
				break;
			}
		}

		if (!outputNode || outputNode->Inputs.empty()) return;

		// Find the first compatible unconnected input on the output node
		NodeGraph::Pin* bestOutput = nullptr;
		NodeGraph::Pin* bestInput = nullptr;

		for (auto& outPin : node->Outputs) {
			for (auto& inPin : outputNode->Inputs) {
				if (!inPin.IsConnected && outPin.CanConnectTo(inPin)) {
					bestOutput = &outPin;
					bestInput = &inPin;
					break;
				}
			}
			if (bestOutput) break;
		}

		// If all inputs are connected, try replacing the first compatible one
		if (!bestOutput) {
			for (auto& outPin : node->Outputs) {
				for (auto& inPin : outputNode->Inputs) {
					if (outPin.CanConnectTo(inPin)) {
						bestOutput = &outPin;
						bestInput = &inPin;
						break;
					}
				}
				if (bestOutput) break;
			}
		}

		if (bestOutput && bestInput) {
			NodeGraph::LinkID linkID = m_Graph->AddLink(bestOutput->ID, bestInput->ID);
			if (linkID != NodeGraph::InvalidLinkID) {
				NotifyGraphChanged();
			}
		}
	}

	// ============================================================================
	// NODE CREATION POPUP
	// ============================================================================

	void NodeEditorPanel::DrawNodeCreationPopup() {
		if (!m_Graph) return;

		if (ImGui::BeginPopup("##CreateNodePopup")) {
			ImGui::Text("Create Node");
			ImGui::Separator();

			// Search filter
			static char searchBuf[256] = "";
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();
			ImGui::InputTextWithHint("##SearchNodes", "Search...", searchBuf, sizeof(searchBuf));
			std::string filterStr = searchBuf;

			ImGui::Separator();

			auto& factory = NodeGraph::NodeFactory::Get();

			// Get nodes for current domain + common nodes
			auto domainNodes = factory.GetNodesForDomain(m_Graph->GetDomain());
			auto commonNodes = factory.GetNodesForDomain(NodeGraph::GraphDomain::None);

			// Merge
			std::vector<const NodeGraph::NodeRegistration*> allNodes;
			allNodes.insert(allNodes.end(), domainNodes.begin(), domainNodes.end());
			allNodes.insert(allNodes.end(), commonNodes.begin(), commonNodes.end());

			// Group by category
			std::unordered_map<std::string, std::vector<const NodeGraph::NodeRegistration*>> categorized;
			for (const auto* reg : allNodes) {
				// Apply search filter
				if (!filterStr.empty()) {
					std::string lowerName = reg->DisplayName;
					std::string lowerFilter = filterStr;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
					std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
					if (lowerName.find(lowerFilter) == std::string::npos)
						continue;
				}
				categorized[reg->Category].push_back(reg);
			}

			// Render categories as tree nodes
			for (auto& [category, nodes] : categorized) {
				if (ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_None)) {
					for (const auto* reg : nodes) {
						if (ImGui::Selectable(reg->DisplayName.c_str())) {
							// Place node at mouse cursor position in grid space
							ImVec2 editorPos = ImNodes::EditorContextGetPanning();
							// Get the editor window position to compute correct offset
							ImVec2 editorOrigin = ImGui::GetWindowPos();
							glm::vec2 gridPos = {
								m_CreateNodePosition.x - editorOrigin.x - editorPos.x,
								m_CreateNodePosition.y - editorOrigin.y - editorPos.y
							};
							CreateNodeAtPosition(reg->TypeName, gridPos);
							searchBuf[0] = '\0';
							ImGui::CloseCurrentPopup();
						}
						if (!reg->Tooltip.empty() && ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", reg->Tooltip.c_str());
						}
					}
					ImGui::TreePop();
				}
			}

			ImGui::EndPopup();
		}
	}

	// ============================================================================
	// PROPERTIES PANEL
	// ============================================================================

	void NodeEditorPanel::DrawPropertiesPanel() {
		ImGui::Text("Properties");
		ImGui::Separator();

		if (m_SelectedNodeID == NodeGraph::InvalidNodeID || !m_Graph) {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a node");
			return;
		}

		auto* node = m_Graph->GetNode(m_SelectedNodeID);
		if (!node) return;

		// Node info
		ImGui::Text("Type: %s", node->TypeName.c_str());
		ImGui::Text("ID: %d", node->ID);
		ImGui::Separator();

		// Display name (editable)
		char nameBuf[128];
		strncpy_s(nameBuf, node->DisplayName.c_str(), sizeof(nameBuf) - 1);
		if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
			node->DisplayName = nameBuf;
		}

		ImGui::Separator();

		// Input pins with editors
		if (!node->Inputs.empty()) {
			ImGui::Text("Inputs");
			for (auto& pin : node->Inputs) {
				if (pin.IsHidden) continue;
				ImGui::PushID(pin.ID);
				ImGui::Text("%s (%s)", pin.Name.c_str(), NodeGraph::PinDataTypeToString(pin.DataType));

				if (!pin.IsConnected) {
					RenderPinDefaultValue(pin);
				}
				else {
					ImGui::TextColored(ImVec4(0.4f, 0.7f, 0.4f, 1.0f), "Connected");
				}
				ImGui::PopID();
			}
		}

		ImGui::Separator();

		// Delegate-specific properties
		if (m_Delegate) {
			m_Delegate->DrawNodeProperties(*node);
		}
	}

	// ============================================================================
	// HELPERS
	// ============================================================================

	void NodeEditorPanel::CreateNodeAtPosition(const std::string& typeName, const glm::vec2& position) {
		if (!m_Graph) return;

		auto node = NodeGraph::NodeFactory::Get().CreateNode(typeName, *m_Graph);
		if (!node) return;

		node->Position = position;

		// Let delegate initialize domain-specific pins
		if (m_Delegate) {
			m_Delegate->OnNodeCreated(*node, *m_Graph);
		}

		m_Graph->AddNode(node);
		NotifyGraphChanged();
	}

	void NodeEditorPanel::NotifyGraphChanged() {
		if (m_Graph) {
			m_Graph->MarkDirty();
			if (m_Delegate) {
				m_Delegate->OnGraphChanged(*m_Graph);
			}
			if (m_OnGraphChanged) {
				m_OnGraphChanged(*m_Graph);
			}
		}
	}

} // namespace Lunex
