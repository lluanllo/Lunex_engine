#pragma once

/**
 * @file NodeEditor.h
 * @brief Master include for the Lunex Node Editor UI (Editor Side)
 * 
 * Include this to access the full node editor UI:
 * - NodeEditorPanel: Main visual editor widget
 * - NodeEditorStyle: Lunex-themed imnodes styling
 * - NodeEditorDelegate: Domain-specific behavior interface
 * 
 * ## Quick Start
 * 
 * ```cpp
 * #include "UI/NodeEditor/NodeEditor.h"
 * #include "NodeGraph/NodeGraphCore.h"
 * 
 * // Create a graph
 * auto graph = CreateRef<NodeGraph::NodeGraph>("MyGraph", NodeGraph::GraphDomain::Shader);
 * 
 * // Create the editor panel
 * NodeEditorPanel m_NodeEditor;
 * m_NodeEditor.SetGraph(graph);
 * m_NodeEditor.SetDelegate(myShaderDelegate);
 * 
 * // In your render loop:
 * m_NodeEditor.OnImGuiRender();
 * ```
 * 
 * ## Creating a Domain-Specific Editor
 * 
 * 1. Create a class inheriting from NodeEditorDelegate
 * 2. Override GetDomain(), OnNodeCreated(), DrawNodeBody(), etc.
 * 3. Register domain-specific nodes with NodeFactory
 * 4. Set your delegate on the NodeEditorPanel
 * 
 * See ShaderGraphDelegate (future) for a complete example.
 */

#include "NodeEditorPanel.h"
#include "NodeEditorStyle.h"
