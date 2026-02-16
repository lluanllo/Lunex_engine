#pragma once

/**
 * @file NodeEditorPanel.h
 * @brief Visual node editor panel using imnodes + LunexUI
 * 
 * This is the main UI component for editing node graphs.
 * It's domain-agnostic - it renders whatever NodeGraph is loaded.
 * Domain-specific behavior (node creation menus, preview, compilation)
 * is delegated to a NodeEditorDelegate.
 */

#include "Core/Core.h"
#include "NodeGraph/NodeGraphCore.h"

#include <imnodes.h>
#include <functional>
#include <string>

namespace Lunex {

	// ============================================================================
	// DELEGATE - Domain-specific behavior
	// ============================================================================

	class NodeEditorDelegate {
	public:
		virtual ~NodeEditorDelegate() = default;

		virtual NodeGraph::GraphDomain GetDomain() const = 0;
		virtual const char* GetEditorTitle() const = 0;

		// Called when a node is created to initialize its pins
		virtual void OnNodeCreated(NodeGraph::Node& node, NodeGraph::NodeGraph& graph) = 0;

		// Custom node body rendering (inline widgets, previews, etc.)
		virtual void DrawNodeBody(NodeGraph::Node& node, NodeGraph::NodeGraph& graph) {}

		// Custom pin value editing widget
		virtual bool DrawPinValueEditor(NodeGraph::Pin& pin) { return false; }

		// Called when the graph changes (for live preview, recompilation, etc.)
		virtual void OnGraphChanged(NodeGraph::NodeGraph& graph) {}

		// Toolbar buttons specific to this domain
		virtual void DrawToolbar(NodeGraph::NodeGraph& graph) {}

		// Properties panel for selected node
		virtual void DrawNodeProperties(NodeGraph::Node& node) {}
	};

	// ============================================================================
	// NODE EDITOR PANEL
	// ============================================================================

	class NodeEditorPanel {
	public:
		NodeEditorPanel();
		~NodeEditorPanel();

		// ========== GRAPH MANAGEMENT ==========

		void SetGraph(Ref<NodeGraph::NodeGraph> graph);
		Ref<NodeGraph::NodeGraph> GetGraph() const { return m_Graph; }

		void SetDelegate(Ref<NodeEditorDelegate> delegate);
		Ref<NodeEditorDelegate> GetDelegate() const { return m_Delegate; }

		// ========== RENDERING ==========

		void OnImGuiRender();
		void OnUpdate(float deltaTime);

		// ========== STATE ==========

		bool IsOpen() const { return m_IsOpen; }
		void SetOpen(bool open) { m_IsOpen = open; }

		void SetTitle(const std::string& title) { m_Title = title; }

		// ========== CALLBACKS ==========

		using GraphChangedCallback = std::function<void(NodeGraph::NodeGraph&)>;
		void SetOnGraphChanged(GraphChangedCallback callback) { m_OnGraphChanged = callback; }

	private:
		// ========== UI DRAWING ==========

		void DrawMenuBar();
		void DrawToolbar();
		void DrawNodeEditor();
		void DrawNodeCreationPopup();
		void DrawPropertiesPanel();
		void DrawMiniMap();

		// ========== NODE RENDERING ==========

		void RenderNode(NodeGraph::Node& node);
		void RenderPin(NodeGraph::Pin& pin, NodeGraph::Node& node);
		void RenderPinDefaultValue(NodeGraph::Pin& pin);
		void RenderLinks();

		// ========== INTERACTION ==========

		void HandleLinkCreation();
		void HandleLinkDeletion();
		void HandleNodeDeletion();
		void HandleContextMenu();

		// ========== HELPERS ==========

		void CreateNodeAtPosition(const std::string& typeName, const glm::vec2& position);
		void NotifyGraphChanged();

	private:
		Ref<NodeGraph::NodeGraph> m_Graph;
		Ref<NodeEditorDelegate> m_Delegate;

		ImNodesEditorContext* m_EditorContext = nullptr;

		// UI state
		bool m_IsOpen = true;
		std::string m_Title = "Node Editor";
		bool m_ShowMiniMap = true;
		bool m_ShowProperties = true;
		bool m_ShowGrid = true;

		// Context menu state
		bool m_OpenCreatePopup = false;
		glm::vec2 m_CreateNodePosition = { 0.0f, 0.0f };
		std::string m_SearchFilter;

		// Selection state
		NodeGraph::NodeID m_SelectedNodeID = NodeGraph::InvalidNodeID;

		// Callbacks
		GraphChangedCallback m_OnGraphChanged;
	};

} // namespace Lunex
