#pragma once

/**
 * @file NodeGraph.h
 * @brief Generic, editor-agnostic node graph (data model + serialization).
 *
 * Shared base for every node-based tool: material graphs today, visual scripting
 * (blueprints) later. A graph only stores nodes, typed pins and links; what a node
 * MEANS is defined by a NodeRegistry for that domain (MaterialGraph registers the
 * material nodes, a future BlueprintGraph registers flow/script nodes).
 *
 * The editor UI lives in Lunex-Editor (UI/Graph/NodeGraphEditor, imgui-node-editor).
 */

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex::Graph {

	using ID = uint64_t;

	enum class PinType : uint8_t {
		Float = 0,
		Vec2,
		Vec3,
		Color,      // vec4
		Texture,    // sampled texture (RGBA) - legacy material graphs
		Flow,       // execution flow (blueprints)
		Any,
		Shader,     // BSDF closure (material graphs)
		Bool,       // blueprints
		Int,
		String,
		Entity
	};

	enum class PinKind : uint8_t { Input = 0, Output };

	const char* PinTypeToString(PinType type);
	PinType PinTypeFromString(const std::string& s);
	bool ArePinTypesCompatible(PinType from, PinType to);

	struct Pin {
		ID PinID = 0;
		std::string Name;
		PinType Type = PinType::Float;
		PinKind Kind = PinKind::Input;
		glm::vec4 DefaultValue{ 0.0f };   // used by unconnected inputs
		glm::vec2 Range{ 0.0f };          // slider range for unconnected Float inputs (0,0 = free drag)
		bool HideValue = false;           // no inline editor (e.g. Normal: implicit geometry normal)
		std::string StringValue;          // String inputs (blueprints)
		std::string Label;                // display name when it differs from Name (unique key)

		const std::string& DisplayName() const { return Label.empty() ? Name : Label; }
	};

	struct Node {
		ID NodeID = 0;
		std::string TypeName;             // registry key, e.g. "Material.TextureSample"
		std::string Title;
		glm::vec2 Position{ 0.0f };
		std::vector<Pin> Inputs;
		std::vector<Pin> Outputs;

		// Node-specific data (constant values, texture path, ...)
		glm::vec4 Value{ 0.0f };
		std::map<std::string, std::string> Properties;

		Pin* FindInput(const std::string& name);
		Pin* FindOutput(const std::string& name);
		const Pin* FindInput(const std::string& name) const;
		const Pin* FindOutput(const std::string& name) const;
	};

	struct Link {
		ID LinkID = 0;
		ID FromPin = 0;   // output
		ID ToPin = 0;     // input
	};

	/**
	 * @brief Describes a node type: how to build its pins and where it appears in menus
	 */
	// Drop-down stored in Node::Properties[Key] (e.g. Math operation, Mix blend mode)
	struct NodeEnumProperty {
		std::string Key;
		std::vector<std::string> Options;
	};

	struct NodeTypeInfo {
		std::string TypeName;
		std::string Title;
		std::string Category;
		bool Unique = false;                         // e.g. the material output node
		std::function<void(Node&)> Build;            // adds pins + default values
		glm::vec4 HeaderColor{ 0.0f };               // 0 = category default of the editor
		float Width = 0.0f;                          // fixed node width (0 = auto)
		bool HiddenInMenu = false;                   // legacy types kept only for loading old files
		std::vector<NodeEnumProperty> Enums;
		std::vector<std::string> Toggles;            // checkboxes stored as "1"/"0" in Properties
		std::string Description;
		// Optional: hide sockets that the current operation does not use (Blender-style)
		std::function<bool(const Node&, const Pin&)> PinVisible;
	};

	class NodeRegistry {
	public:
		void Register(const NodeTypeInfo& info) { m_Types[info.TypeName] = info; m_Order.push_back(info.TypeName); }
		const NodeTypeInfo* Get(const std::string& typeName) const;
		const std::vector<std::string>& GetOrder() const { return m_Order; }

	private:
		std::map<std::string, NodeTypeInfo> m_Types;
		std::vector<std::string> m_Order;
	};

	class NodeGraph {
	public:
		explicit NodeGraph(const NodeRegistry* registry = nullptr) : m_Registry(registry) {}

		void SetRegistry(const NodeRegistry* registry) { m_Registry = registry; }
		const NodeRegistry* GetRegistry() const { return m_Registry; }

		// ---- Nodes ----
		Node* AddNode(const std::string& typeName, const glm::vec2& position = glm::vec2(0.0f));
		bool RemoveNode(ID nodeID);
		// Copy of a node (values, properties, input defaults) with fresh IDs; links are not copied
		Node* DuplicateNode(ID nodeID, const glm::vec2& offset = glm::vec2(40.0f));
		Node* FindNode(ID nodeID);
		const Node* FindNode(ID nodeID) const;
		Node* FindNodeByType(const std::string& typeName);
		const Node* FindNodeByType(const std::string& typeName) const;
		std::vector<Node>& GetNodes() { return m_Nodes; }
		const std::vector<Node>& GetNodes() const { return m_Nodes; }

		// ---- Pins ----
		Pin* FindPin(ID pinID);
		const Pin* FindPin(ID pinID) const;
		Node* FindPinOwner(ID pinID);
		const Node* FindPinOwner(ID pinID) const;

		// ---- Links ----
		// Validates kinds/types; an input accepts one link (the old one is replaced)
		bool CanConnect(ID a, ID b) const;
		const Link* Connect(ID a, ID b);
		bool RemoveLink(ID linkID);
		const Link* GetLinkInto(ID inputPin) const;
		const std::vector<Link>& GetLinks() const { return m_Links; }

		// Follow the link into an input: returns the source node/pin (or nulls)
		std::pair<const Node*, const Pin*> GetSource(ID inputPin) const;

		// ---- Change tracking ----
		uint64_t GetVersion() const { return m_Version; }
		void Touch() { m_Version++; }

		// ---- Serialization ----
		void Serialize(YAML::Emitter& out) const;
		bool Deserialize(const YAML::Node& node);

	private:
		ID NextID() { return m_NextID++; }
		void AssignPinIDs(Node& node);

		const NodeRegistry* m_Registry = nullptr;
		std::vector<Node> m_Nodes;
		std::vector<Link> m_Links;
		ID m_NextID = 1;
		uint64_t m_Version = 1;
	};

} // namespace Lunex::Graph
