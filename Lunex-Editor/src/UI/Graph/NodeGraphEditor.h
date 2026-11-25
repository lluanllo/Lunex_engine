#pragma once

/**
 * @file NodeGraphEditor.h
 * @brief Generic node graph canvas (imgui-node-editor) for any Graph::NodeGraph.
 *
 * Domain panels (Material Editor today, Blueprint editor later) own one of these and
 * customize it through callbacks:
 *   - DrawNodeBody: widgets inside a node (constants, texture pickers...)
 *   - pin colours come from the pin type
 * Right-click the background to add nodes (from the graph's NodeRegistry), drag
 * between pins to link, Del removes the selection, F frames the graph.
 */

#include "Graph/NodeGraph.h"

#include <functional>
#include <string>

namespace ax::NodeEditor { struct EditorContext; }

namespace Lunex::UI {

	class NodeGraphEditor {
	public:
		explicit NodeGraphEditor(const std::string& id);
		~NodeGraphEditor();

		NodeGraphEditor(const NodeGraphEditor&) = delete;
		NodeGraphEditor& operator=(const NodeGraphEditor&) = delete;

		// Returns true when the graph changed (link/node added/removed, value edited)
		bool Draw(Graph::NodeGraph& graph, const char* emptyHint = nullptr);

		// Return true if a value inside the node changed
		std::function<bool(Graph::Node&)> DrawNodeBody;

		void RequestFrameAll() { m_FrameDelay = 2; }   // after node sizes are known

	private:
		void DrawNode(Graph::NodeGraph& graph, Graph::Node& node, bool& changed);
		bool DrawInputDefault(Graph::Pin& pin);
		void HandleCreation(Graph::NodeGraph& graph, bool& changed);
		void HandleDeletion(Graph::NodeGraph& graph, bool& changed);
		void DrawContextMenu(Graph::NodeGraph& graph, bool& changed);

		std::string m_ID;
		ax::NodeEditor::EditorContext* m_Context = nullptr;
		const Graph::NodeGraph* m_LastGraph = nullptr;
		bool m_PositionsDirty = true;      // push node positions into the editor
		int m_FrameDelay = 2;
	};

} // namespace Lunex::UI
