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
 *  - X / Delete: delete, Shift+D: duplicate, F: frame, Home: frame all,
 *    middle mouse: pan, wheel: zoom
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
		bool Draw(Graph::NodeGraph& graph, const char* emptyHint = nullptr);

		void RequestFrameAll() { m_FrameDelay = 2; }

		// Selection (first selected node, 0 = none)
		Graph::ID GetSelectedNode() const { return m_SelectedNode; }

		// Custom node body, drawn between the drop-downs and the inputs. Return true if changed.
		std::function<bool(Graph::Node&)> DrawNodeBody;

		// Opens a popup at the mouse (popups can't be opened from inside the canvas directly)
		void RequestPopup(std::function<bool(Graph::NodeGraph&)> content);

		// Blender-like inline editors (also usable from DrawNodeBody callbacks)
		static bool SliderField(const char* id, const char* label, float* value, float min, float max, float width, bool isInt = false);
		bool ColorField(Graph::ID nodeID, Graph::ID pinID, glm::vec4& color, float width, float height = 0.0f);

	private:
		void DrawNode(Graph::NodeGraph& graph, Graph::Node& node, bool& changed);
		bool DrawInputValue(Graph::Node& node, Graph::Pin& pin, float width);
		void DrawSocket(const Graph::Pin& pin, bool connected, const ImVec2& center);
		void HandleCreation(Graph::NodeGraph& graph, bool& changed);
		void HandleDeletion(Graph::NodeGraph& graph, bool& changed);
		void HandleShortcuts(Graph::NodeGraph& graph, bool& changed);
		void DrawPopups(Graph::NodeGraph& graph, bool& changed);
		bool DrawAddMenu(Graph::NodeGraph& graph);
		Graph::Node* SpawnNode(Graph::NodeGraph& graph, const std::string& typeName);

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
	};

} // namespace Lunex::UI
