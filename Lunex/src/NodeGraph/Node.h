#pragma once

/**
 * @file Node.h
 * @brief Base node class for the node graph system
 * 
 * Nodes are the fundamental building blocks of all graph-based editors.
 * Each domain (shader, animation, audio, blueprint) creates specialized
 * node types by inheriting from this base.
 * 
 * Design:
 * - Nodes own their pins
 * - Nodes have a type identifier for factory reconstruction
 * - Nodes can evaluate to produce output values
 * - Nodes serialize/deserialize for persistence
 */

#include "NodeGraphTypes.h"
#include "Pin.h"

#include <glm/glm.hpp>

namespace Lunex::NodeGraph {

	class NodeGraph; // Forward declaration

	class Node {
	public:
		Node() = default;
		Node(NodeID id, const std::string& typeName)
			: ID(id), TypeName(typeName) {}
		virtual ~Node() = default;

		// ========== IDENTIFICATION ==========
		NodeID ID = InvalidNodeID;
		std::string TypeName;     // e.g. "Shader::Add", "Blueprint::Branch"
		std::string DisplayName;  // e.g. "Add", "Branch" (shown in UI)
		std::string Category;     // e.g. "Math", "Utility"

		// ========== PINS ==========
		std::vector<Pin> Inputs;
		std::vector<Pin> Outputs;

		// ========== VISUAL STATE ==========
		glm::vec2 Position = { 0.0f, 0.0f };
		uint32_t HeaderColor = PackColor(60, 60, 60);
		NodeFlags Flags = NodeFlags::None;

		// ========== STATUS ==========
		NodeStatus Status = NodeStatus::None;
		std::string StatusMessage;

		// ========== PIN MANAGEMENT ==========

		Pin* FindPin(PinID pinID) {
			for (auto& pin : Inputs)
				if (pin.ID == pinID) return &pin;
			for (auto& pin : Outputs)
				if (pin.ID == pinID) return &pin;
			return nullptr;
		}

		const Pin* FindPin(PinID pinID) const {
			for (const auto& pin : Inputs)
				if (pin.ID == pinID) return &pin;
			for (const auto& pin : Outputs)
				if (pin.ID == pinID) return &pin;
			return nullptr;
		}

		Pin& AddInput(PinID id, const std::string& name, PinDataType type, PinValue defaultValue = {}) {
			auto& pin = Inputs.emplace_back(id, name, type, PinDirection::Input);
			pin.OwnerNodeID = ID;
			pin.DefaultValue = defaultValue;
			return pin;
		}

		Pin& AddOutput(PinID id, const std::string& name, PinDataType type) {
			auto& pin = Outputs.emplace_back(id, name, type, PinDirection::Output);
			pin.OwnerNodeID = ID;
			return pin;
		}

		// ========== VIRTUAL INTERFACE ==========
		
		virtual GraphDomain GetDomain() const { return GraphDomain::None; }
		
		virtual std::string GetTooltip() const { return DisplayName; }

		virtual bool Validate() { return true; }
	};

} // namespace Lunex::NodeGraph
