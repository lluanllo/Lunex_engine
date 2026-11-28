#pragma once

/**
 * @file NodeGraphEditor.h
 * @brief Blender-style canvas for any Graph::NodeGraph (imgui-node-editor).
 *
 * Shared by the material node editor and the blueprint (visual scripting) editor.
 *
 *  - Colored headers per category, sockets on the node border, links tinted by type
 *  - Inline socket values: Blender-like slider fields (bounded = proportional fill),
 *    color swatches with picker, vector X/Y/Z fields, string fields
 *  - Drop-downs / toggles declared in NodeTypeInfo (Math operation, blend mode...)
 *  - Dragging from a connected input detaches its link (drop on another input to move
 *    it, on empty space to remove it), like Blender
 *  - Shift+A / right-click: add menu with categories + search. Releasing a new link on
 *    empty space opens the menu and connects the new node.
 *  - X / Delete: delete, Shift+D: duplicate, Period: frame selected, Home: frame all,
 *    middle mouse: pan, wheel: zoom
 *  - Node Wrangler: F / Shift+F make links, Ctrl+X dissolve, Alt+RMB drag lazy connect,
 *    Alt+S swap inputs, Ctrl+H hide unused sockets, Ctrl+Shift+Click preview output,
 *    Ctrl+T / Ctrl+Shift+T texture setups (hooks provided by the owning panel)
 *  - G: grab (move) the selected nodes or the hovered one (reroutes too); LMB / Enter
 *    confirms, RMB / Esc cancels
 *  - Shift + right drag over links: one reroute per source socket (links leaving the same
 *    output share it and fork from there)
 *  - Node previews float above their node (they are not part of the node body)
 */

#include "Graph/NodeGraph.h"

#include <imgui.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ax::NodeEditor { struct EditorContext; }

namespace Lunex::UI {

	ImVec4 PinTypeColor(Graph::PinType type);

	class NodeGraphEditor {
	public:
		explicit NodeGraphEditor(const std::string& id);
		~NodeGraphEditor();

		NodeGraphEditor(const NodeGraphEditor&) = delete;
		NodeGraphEditor& operator=(const NodeGraphEditor&) = delete;

		// Draws the canvas filling the current content region. Returns true when the graph changed.
		// `graph` is the root graph: when a group is being edited, its subgraph is drawn instead.
		bool Draw(Graph::NodeGraph& graph, const char* emptyHint = nullptr);

		void RequestFrameAll() { m_FrameDelay = 2; }

		// Selection (first selected node, 0 = none) - the node lives in GetCurrentGraph()
		Graph::ID GetSelectedNode() const { return m_SelectedNode; }

		// Node groups (Ctrl+G make, Tab enter/exit, Ctrl+Alt+G ungroup)
		bool EnableGroups = false;
		Graph::NodeGraph* GetCurrentGraph() const { return m_CurrentGraph; }
		bool IsInsideGroup() const { return !m_GroupPath.empty(); }
		void ExitAllGroups() { m_GroupPath.clear(); }
		// The graph being edited (root, or the subgraph of the entered group)
		Graph::NodeGraph& ResolveCurrentGraph(Graph::NodeGraph& root);

		// Custom node body, drawn between the drop-downs and the inputs. Return true if changed.
		std::function<bool(Graph::Node&)> DrawNodeBody;
		// Optional preview image drawn above a node (Blender node previews): return false for none
		std::function<bool(Graph::Node&, ImTextureID& texture, ImVec2& uv0, ImVec2& uv1)> GetNodePreview;
		// Something dropped on the empty canvas (e.g. an image from the Content Browser), at `position`
		// in canvas space. Return true when nodes were added.
		std::function<bool(Graph::NodeGraph&, const ImGuiPayload& payload, const glm::vec2& position)> OnCanvasDrop;
		std::string CanvasDropPayload = "CONTENT_BROWSER_ITEM";

		// Node Wrangler hooks (domain specific; return true when the graph changed)
		std::function<bool(Graph::NodeGraph&, Graph::ID node, int outputIndex)> OnPreviewNode;   // Ctrl+Shift+Click
		std::function<bool(Graph::NodeGraph&, Graph::ID node)> OnTextureSetup;                   // Ctrl+T
		std::function<bool(Graph::NodeGraph&, Graph::ID node)> OnPrincipledTextureSetup;         // Ctrl+Shift+T

		// Opens a popup at the mouse (popups can't be opened from inside the canvas directly)
		void RequestPopup(std::function<bool(Graph::NodeGraph&)> content);

		// Blender-like inline editors (also usable from DrawNodeBody callbacks)
		static bool SliderField(const char* id, const char* label, float* value, float min, float max, float width, bool isInt = false);
		bool ColorField(Graph::ID nodeID, Graph::ID pinID, glm::vec4& color, float width, float height = 0.0f);

	private:
		void DrawNode(Graph::NodeGraph& graph, Graph::Node& node, bool& changed);
		void DrawReroute(Graph::NodeGraph& graph, Graph::Node& node, bool linked);
		void HandleCanvasTools(Graph::NodeGraph& graph, bool& changed);
		void DrawOverlays(Graph::NodeGraph& root);
		// Link curve in canvas space (same easing as imgui-node-editor), `segments` + 1 points
		void LinkCurve(const Graph::Link& link, std::vector<ImVec2>& points, int segments = 24) const;
		bool DrawInputValue(Graph::Node& node, Graph::Pin& pin, float width);
		void DrawSocket(const Graph::Pin& pin, bool connected, const ImVec2& center);
		void HandleCreation(Graph::NodeGraph& graph, bool& changed);
		void HandleDeletion(Graph::NodeGraph& graph, bool& changed);
		void HandleShortcuts(Graph::NodeGraph& graph, bool& changed);
		void HandleGrab(Graph::NodeGraph& graph, bool& changed);
		void DrawPopups(Graph::NodeGraph& graph, bool& changed);
		bool DrawAddMenu(Graph::NodeGraph& graph);
		Graph::Node* SpawnNode(Graph::NodeGraph& graph, const std::string& typeName);
		bool MakeLinks(Graph::NodeGraph& graph, std::vector<Graph::ID> nodes, bool replace, bool keepOrder = false);
		bool DissolveNode(Graph::NodeGraph& graph, Graph::ID node);
		bool SwapInputs(Graph::NodeGraph& graph, Graph::ID node);

		ax::NodeEditor::EditorContext* m_Context = nullptr;
		std::string m_ID;
		Graph::NodeGraph* m_LastGraph = nullptr;
		bool m_PositionsDirty = true;
		int m_FrameDelay = 2;
		Graph::ID m_SelectedNode = 0;

		// Socket centers in canvas space (for the detached-link preview)
		std::unordered_map<Graph::ID, ImVec2> m_SocketPos;

		// Add menu
		bool m_OpenAddMenu = false;
		ImVec2 m_SpawnPos{ 0, 0 };
		char m_Search[64] = {};
		Graph::ID m_PendingLinkPin = 0;   // connect the spawned node to this pin

		// Generic popup
		bool m_OpenPopup = false;
		std::function<bool(Graph::NodeGraph&)> m_PopupContent;

		// Link being dragged out of a connected input (Blender detach)
		bool m_Creating = false;
		Graph::ID m_DragStartPin = 0;
		Graph::ID m_DetachSource = 0;

		// Node Wrangler state
		Graph::ID m_LazyFrom = 0;
		bool m_SuppressContextMenu = false;
		Graph::ID m_PreviewNode = 0;
		int m_PreviewOutput = 0;

		// G grab: nodes moved with the mouse until confirmed / cancelled
		bool m_Grabbing = false;
		ImVec2 m_GrabStart{ 0, 0 };
		std::vector<std::pair<Graph::ID, ImVec2>> m_GrabNodes;   // node, position before the grab
		bool m_SwallowClick = false;                               // the click that ended a grab
		ImVec2 m_CanvasMouse{ 0, 0 };                              // mouse in canvas space (last frame)

		// Groups
		std::vector<Graph::ID> m_GroupPath;            // group nodes entered, from the root
		Graph::NodeGraph* m_CurrentGraph = nullptr;

		// Canvas tools (canvas space points; overlays are drawn above the nodes after ed::End)
		enum class Stroke { None, Knife, Reroute };
		Stroke m_Stroke = Stroke::None;
		std::vector<ImVec2> m_StrokePoints;
		bool m_BoxSelecting = false;
		ImVec2 m_BoxStart{ 0, 0 };
		Graph::ID m_PressNode = 0;                     // node under the mouse when the left button went down
		Graph::ID m_InsertLink = 0;                    // link highlighted for "drop node on link"
		bool m_DraggingColor = false;
		glm::vec4 m_DragColor{ 1.0f };
		// Screen-space copies for the overlay pass
		std::vector<ImVec2> m_ScreenStroke, m_ScreenInsert, m_ScreenLazy;
		ImVec2 m_ScreenBoxA{ 0, 0 }, m_ScreenBoxB{ 0, 0 };
	};

} // namespace Lunex::UI
