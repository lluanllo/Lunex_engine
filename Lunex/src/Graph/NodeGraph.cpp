#include "stpch.h"
#include "NodeGraph.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>

namespace Lunex::Graph {

	// ============================================================================
	// PIN TYPES
	// ============================================================================

	const char* PinTypeToString(PinType type) {
		switch (type) {
			case PinType::Float:   return "Float";
			case PinType::Vec2:    return "Vec2";
			case PinType::Vec3:    return "Vec3";
			case PinType::Color:   return "Color";
			case PinType::Texture: return "Texture";
			case PinType::Flow:    return "Flow";
			default:               return "Any";
		}
	}

	PinType PinTypeFromString(const std::string& s) {
		if (s == "Float") return PinType::Float;
		if (s == "Vec2") return PinType::Vec2;
		if (s == "Vec3") return PinType::Vec3;
		if (s == "Color") return PinType::Color;
		if (s == "Texture") return PinType::Texture;
		if (s == "Flow") return PinType::Flow;
		return PinType::Any;
	}

	bool ArePinTypesCompatible(PinType from, PinType to) {
		if (from == PinType::Flow || to == PinType::Flow) return from == to;
		if (from == PinType::Any || to == PinType::Any) return true;
		if (from == to) return true;
		// A sampled texture can feed color/vector inputs
		if (from == PinType::Texture) return to == PinType::Color || to == PinType::Vec3;
		if (to == PinType::Texture) return false;
// Numeric values convert implicitly (float splats, vectors truncate/extend)
		auto numeric = [](PinType t) { return t == PinType::Float || t == PinType::Vec2 || t == PinType::Vec3 || t == PinType::Color; };
		return numeric(from) && numeric(to);
	}

	// ============================================================================
	// NODE
	// ============================================================================

	template<typename V>
	static auto FindPinByName(V& pins, const std::string& name) -> decltype(&pins[0]) {
		for (auto& p : pins) if (p.Name == name) return &p;
		return nullptr;
	}

	Pin* Node::FindInput(const std::string& name) { return FindPinByName(Inputs, name); }
	Pin* Node::FindOutput(const std::string& name) { return FindPinByName(Outputs, name); }
	const Pin* Node::FindInput(const std::string& name) const { return FindPinByName(Inputs, name); }
	const Pin* Node::FindOutput(const std::string& name) const { return FindPinByName(Outputs, name); }

	const NodeTypeInfo* NodeRegistry::Get(const std::string& typeName) const {
		auto it = m_Types.find(typeName);
		return it != m_Types.end() ? &it->second : nullptr;
	}

	// ============================================================================
	// GRAPH
	// ============================================================================

	void NodeGraph::AssignPinIDs(Node& node) {
		for (auto& p : node.Inputs) { p.Kind = PinKind::Input; if (p.PinID == 0) p.PinID = NextID(); }
		for (auto& p : node.Outputs) { p.Kind = PinKind::Output; if (p.PinID == 0) p.PinID = NextID(); }
	}

	Node* NodeGraph::AddNode(const std::string& typeName, const glm::vec2& position) {
		const NodeTypeInfo* info = m_Registry ? m_Registry->Get(typeName) : nullptr;
		if (!info) return nullptr;
		if (info->Unique && FindNodeByType(typeName)) return FindNodeByType(typeName);

		Node node;
		node.NodeID = NextID();
		node.TypeName = typeName;
		node.Title = info->Title;
		node.Position = position;
		if (info->Build) info->Build(node);
		AssignPinIDs(node);
		m_Nodes.push_back(std::move(node));
		Touch();
		return &m_Nodes.back();
	}

	bool NodeGraph::RemoveNode(ID nodeID) {
		auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(), [&](const Node& n) { return n.NodeID == nodeID; });
		if (it == m_Nodes.end()) return false;
		const NodeTypeInfo* info = m_Registry ? m_Registry->Get(it->TypeName) : nullptr;
		if (info && info->Unique) return false;   // the output node can't be deleted

		auto ownsPin = [&](ID pin) {
			for (const auto& p : it->Inputs) if (p.PinID == pin) return true;
			for (const auto& p : it->Outputs) if (p.PinID == pin) return true;
			return false;
		};
		m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
			[&](const Link& l) { return ownsPin(l.FromPin) || ownsPin(l.ToPin); }), m_Links.end());
		m_Nodes.erase(it);
		Touch();
		return true;
	}

	Node* NodeGraph::FindNode(ID nodeID) {
		for (auto& n : m_Nodes) if (n.NodeID == nodeID) return &n;
		return nullptr;
	}
	const Node* NodeGraph::FindNode(ID nodeID) const {
		for (const auto& n : m_Nodes) if (n.NodeID == nodeID) return &n;
		return nullptr;
	}
	Node* NodeGraph::FindNodeByType(const std::string& typeName) {
		for (auto& n : m_Nodes) if (n.TypeName == typeName) return &n;
		return nullptr;
	}
	const Node* NodeGraph::FindNodeByType(const std::string& typeName) const {
		for (const auto& n : m_Nodes) if (n.TypeName == typeName) return &n;
		return nullptr;
	}

	Pin* NodeGraph::FindPin(ID pinID) {
		for (auto& n : m_Nodes) {
			for (auto& p : n.Inputs) if (p.PinID == pinID) return &p;
			for (auto& p : n.Outputs) if (p.PinID == pinID) return &p;
		}
		return nullptr;
	}
	const Pin* NodeGraph::FindPin(ID pinID) const { return const_cast<NodeGraph*>(this)->FindPin(pinID); }

	Node* NodeGraph::FindPinOwner(ID pinID) {
		for (auto& n : m_Nodes) {
			for (auto& p : n.Inputs) if (p.PinID == pinID) return &n;
			for (auto& p : n.Outputs) if (p.PinID == pinID) return &n;
		}
		return nullptr;
	}
	const Node* NodeGraph::FindPinOwner(ID pinID) const { return const_cast<NodeGraph*>(this)->FindPinOwner(pinID); }

	bool NodeGraph::CanConnect(ID a, ID b) const {
		const Pin* pa = FindPin(a);
		const Pin* pb = FindPin(b);
		if (!pa || !pb || pa->Kind == pb->Kind) return false;
		const Pin* out = pa->Kind == PinKind::Output ? pa : pb;
		const Pin* in = pa->Kind == PinKind::Input ? pa : pb;
		if (FindPinOwner(out->PinID) == FindPinOwner(in->PinID)) return false;
		return ArePinTypesCompatible(out->Type, in->Type);
	}

	const Link* NodeGraph::Connect(ID a, ID b) {
		if (!CanConnect(a, b)) return nullptr;
		const Pin* pa = FindPin(a);
		ID from = pa->Kind == PinKind::Output ? a : b;
		ID to = pa->Kind == PinKind::Output ? b : a;

		// One link per input
		m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return l.ToPin == to; }), m_Links.end());
		m_Links.push_back({ NextID(), from, to });
		Touch();
		return &m_Links.back();
	}

	bool NodeGraph::RemoveLink(ID linkID) {
		auto it = std::find_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return l.LinkID == linkID; });
		if (it == m_Links.end()) return false;
		m_Links.erase(it);
		Touch();
		return true;
	}

	const Link* NodeGraph::GetLinkInto(ID inputPin) const {
		for (const auto& l : m_Links) if (l.ToPin == inputPin) return &l;
		return nullptr;
	}

	std::pair<const Node*, const Pin*> NodeGraph::GetSource(ID inputPin) const {
		const Link* link = GetLinkInto(inputPin);
		if (!link) return { nullptr, nullptr };
		return { FindPinOwner(link->FromPin), FindPin(link->FromPin) };
	}

	// ============================================================================
	// SERIALIZATION
	// ============================================================================

	static void EmitVec4(YAML::Emitter& out, const glm::vec4& v) {
		out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	}
	static glm::vec4 ReadVec4(const YAML::Node& n, const glm::vec4& fallback) {
		if (!n || !n.IsSequence() || n.size() < 4) return fallback;
		return { n[0].as<float>(), n[1].as<float>(), n[2].as<float>(), n[3].as<float>() };
	}

	void NodeGraph::Serialize(YAML::Emitter& out) const {
		out << YAML::BeginMap;
		out << YAML::Key << "NextID" << YAML::Value << m_NextID;
		out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
		for (const auto& n : m_Nodes) {
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << n.NodeID;
			out << YAML::Key << "Type" << YAML::Value << n.TypeName;
			out << YAML::Key << "Position" << YAML::Value << YAML::Flow << YAML::BeginSeq << n.Position.x << n.Position.y << YAML::EndSeq;
			out << YAML::Key << "Value" << YAML::Value; EmitVec4(out, n.Value);
			if (!n.Properties.empty()) {
				out << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
				for (const auto& [k, v] : n.Properties) out << YAML::Key << k << YAML::Value << v;
				out << YAML::EndMap;
			}
			out << YAML::Key << "Pins" << YAML::Value << YAML::BeginSeq;
			auto emitPins = [&](const std::vector<Pin>& pins, const char* kind) {
				for (const auto& p : pins) {
					out << YAML::Flow << YAML::BeginMap;
					out << YAML::Key << "Name" << YAML::Value << p.Name;
					out << YAML::Key << "Kind" << YAML::Value << kind;
					out << YAML::Key << "ID" << YAML::Value << p.PinID;
					if (p.Kind == PinKind::Input) { out << YAML::Key << "Default" << YAML::Value; EmitVec4(out, p.DefaultValue); }
					out << YAML::EndMap;
				}
			};
			emitPins(n.Inputs, "In");
			emitPins(n.Outputs, "Out");
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::Key << "Links" << YAML::Value << YAML::BeginSeq;
		for (const auto& l : m_Links)
			out << YAML::Flow << YAML::BeginSeq << l.LinkID << l.FromPin << l.ToPin << YAML::EndSeq;
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}

	bool NodeGraph::Deserialize(const YAML::Node& data) {
		if (!data || !data.IsMap()) return false;
		m_Nodes.clear();
		m_Links.clear();
		m_NextID = data["NextID"].as<uint64_t>(1);

		if (auto nodes = data["Nodes"]; nodes && nodes.IsSequence()) {
			for (const auto& nd : nodes) {
				Node node;
				node.NodeID = nd["ID"].as<uint64_t>(0);
				node.TypeName = nd["Type"].as<std::string>("");
				if (auto pos = nd["Position"]; pos && pos.size() >= 2) node.Position = { pos[0].as<float>(), pos[1].as<float>() };

				// Rebuild pins from the registry so newer node versions gain their pins
				const NodeTypeInfo* info = m_Registry ? m_Registry->Get(node.TypeName) : nullptr;
				if (!info) continue;   // unknown node type: dropped (its links are dropped too)
				node.Title = info->Title;
				if (info->Build) info->Build(node);
				node.Value = ReadVec4(nd["Value"], node.Value);
				if (auto props = nd["Properties"]; props && props.IsMap())
					for (auto it = props.begin(); it != props.end(); ++it)
						node.Properties[it->first.as<std::string>()] = it->second.as<std::string>("");

				// Restore pin IDs/defaults by name
				if (auto pins = nd["Pins"]; pins && pins.IsSequence()) {
					for (const auto& pd : pins) {
						std::string name = pd["Name"].as<std::string>("");
						bool isInput = pd["Kind"].as<std::string>("In") == "In";
						Pin* pin = isInput ? node.FindInput(name) : node.FindOutput(name);
						if (!pin) continue;
						pin->PinID = pd["ID"].as<uint64_t>(0);
						if (isInput) pin->DefaultValue = ReadVec4(pd["Default"], pin->DefaultValue);
					}
				}
				if (node.NodeID == 0) node.NodeID = NextID();
				AssignPinIDs(node);
				m_Nodes.push_back(std::move(node));
			}
		}

		if (auto links = data["Links"]; links && links.IsSequence()) {
			for (const auto& ld : links) {
				if (!ld.IsSequence() || ld.size() < 3) continue;
				Link link{ ld[0].as<uint64_t>(), ld[1].as<uint64_t>(), ld[2].as<uint64_t>() };
				if (FindPin(link.FromPin) && FindPin(link.ToPin)) m_Links.push_back(link);
			}
		}

		// Keep NextID above every ID in use
		ID maxID = 0;
		for (const auto& n : m_Nodes) {
			maxID = std::max(maxID, n.NodeID);
			for (const auto& p : n.Inputs) maxID = std::max(maxID, p.PinID);
			for (const auto& p : n.Outputs) maxID = std::max(maxID, p.PinID);
		}
		for (const auto& l : m_Links) maxID = std::max(maxID, l.LinkID);
		m_NextID = std::max(m_NextID, maxID + 1);
		Touch();
		return true;
	}

} // namespace Lunex::Graph
