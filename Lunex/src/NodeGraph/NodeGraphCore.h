#pragma once

/**
 * @file NodeGraphCore.h
 * @brief Master include for the Lunex Node Graph System (Engine Side)
 * 
 * Include this to access the full node graph data model:
 * - NodeGraphTypes: Enums, IDs, type definitions
 * - Pin: Connection point with typed data
 * - Node: Base node with pins
 * - Link: Connection between pins
 * - NodeGraph: Graph data structure with topological sort
 * - NodeFactory: Node type registration and creation
 * - NodeGraphSerializer: YAML persistence
 * 
 * ## Architecture
 * 
 * The node graph system is split into two layers:
 * 
 * ### Engine Layer (Lunex/src/NodeGraph/)
 * Domain-agnostic data model. No UI dependencies.
 * - NodeGraph, Node, Pin, Link - data structures
 * - NodeFactory - type registration
 * - NodeGraphSerializer - YAML persistence
 * 
 * ### Editor Layer (Lunex-Editor/src/UI/NodeEditor/)
 * UI presentation using imnodes + LunexUI.
 * - NodeEditorPanel - main editor widget
 * - NodeEditorStyle - Lunex-themed styling
 * - Domain-specific editors (ShaderGraphEditor, etc.)
 * 
 * ## Domains
 * 
 * Each domain creates specialized nodes by:
 * 1. Defining node classes that extend Node
 * 2. Registering them with NodeFactory
 * 3. Optionally providing a domain-specific compiler/evaluator
 * 
 * Supported domains:
 * - Shader: Visual shader authoring ? GLSL generation
 * - Animation: State machines, blend trees
 * - Audio: Mixer chains, DSP processing
 * - Blueprint: Visual scripting with flow control
 * - BehaviorTree: AI decision making (future)
 * - Particle: Particle system graphs (future)
 * - PostProcess: Post-processing effect chains (future)
 */

#include "NodeGraphTypes.h"
#include "Pin.h"
#include "Node.h"
#include "Link.h"
#include "NodeGraph.h"
#include "NodeFactory.h"
#include "NodeGraphSerializer.h"
