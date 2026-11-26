#include "stpch.h"
#include "NodeGraph.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
			case PinType::Shader:  return "Shader";
			case PinType::Bool:    return "Bool";
			case PinType::Int:     return "Int";
			case PinType::String:  return "String";
			case PinType::Entity:  return "Entity";
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
		if (s == "Shader") return PinType::Shader;
		if (s == "Bool") return PinType::Bool;
		if (s == "Int") return PinType::Int;
		if (s == "String") return PinType::String;
		if (s == "Entity") return PinType::Entity;
		return PinType::Any;
	}

	bool ArePinTypesCompatible(PinType from, PinType to) {
		if (from == PinType::Flow || to == PinType::Flow) return from == to;
		if (from == PinType::Any || to == PinType::Any) return true;
		if (from == to) return true;
		// Closures only connect to closures; strings/entities only to themselves
		if (from == PinType::Shader || to == PinType::Shader) return false;
		if (from == PinType::String || to == PinType::String || from == PinType::Entity || to == PinType::Entity) return false;
		// A sampled texture can feed color/vector inputs
		if (from == PinType::Texture) return to == PinType::Color || to == PinType::Vec3 || to == PinType::Float;
		if (to == PinType::Texture) return from == PinType::Color || from == PinType::Vec3;
		// Numeric values convert implicitly (float splats, vectors truncate/extend, bool/int <-> float)
		auto numeric = [](PinType t) {
			return t == PinType::Float || t == PinType::Vec2 || t == PinType::Vec3 || t == PinType::Color || t == PinType::Bool || t == PinType::Int;
		};
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
	// BUILT-IN TYPES
	// ============================================================================

	const NodeRegistry& NodeGraph::GetBuiltinRegistry() {
		static const NodeRegistry registry = [] {
			NodeRegistry r;
			NodeTypeInfo reroute;
			reroute.TypeName = RerouteType;
			reroute.Title = "Reroute";
			reroute.Category = "Layout";
			reroute.Width = 16.0f;
			reroute.Description = "Point on a link to organize the graph (Shift + right drag over links)";
			r.Register(reroute);

			NodeTypeInfo group;
			group.TypeName = GroupType;
			group.Title = "Group";
			group.Category = "Group";
			group.HiddenInMenu = true;
			group.Width = 170.0f;
			r.Register(group);

			NodeTypeInfo in;
			in.TypeName = GroupInputType;
			in.Title = "Group Input";
			in.Category = "Group";
			in.HiddenInMenu = true;
			in.Unique = true;
			in.Width = 150.0f;
			r.Register(in);

			NodeTypeInfo out = in;
			out.TypeName = GroupOutputType;
			out.Title = "Group Output";
			r.Register(out);
			return r;
		}();
		return registry;
	}

	const NodeTypeInfo* NodeGraph::GetTypeInfo(const std::string& typeName) const {
		if (m_Registry) if (const NodeTypeInfo* info = m_Registry->Get(typeName)) return info;
		return GetBuiltinRegistry().Get(typeName);
	}

	void NodeGraph::SetRegistry(const NodeRegistry* registry) {
		m_Registry = registry;
		for (auto& n : m_Nodes) if (n.Subgraph) n.Subgraph->SetRegistry(registry);
	}

	void NodeGraph::BuildBuiltinPins(Node& node) {
		auto pin = [](const std::string& name, PinType type) { Pin p; p.Name = name; p.Type = type; return p; };
		if (node.TypeName == RerouteType) {
			if (node.Inputs.empty()) node.Inputs.push_back(pin("Input", PinType::Any));
			if (node.Outputs.empty()) node.Outputs.push_back(pin("Output", PinType::Any));
		}
		else if (node.TypeName == GroupInputType || node.TypeName == GroupOutputType) {
			const bool isInput = node.TypeName == GroupInputType;
			const auto& sockets = isInput ? m_InterfaceInputs : m_InterfaceOutputs;
			std::vector<Pin>& pins = isInput ? node.Outputs : node.Inputs;
			std::vector<Pin> rebuilt;
			for (const auto& s : sockets) {
				Pin p = pin(s.Name, s.Type);
				p.HideValue = true;
				if (const Pin* old = FindPinByName(pins, s.Name)) p.PinID = old->PinID;
				rebuilt.push_back(p);
			}
			Pin extra = pin(NewSocketPin, PinType::Any);
			extra.Label = " ";
			extra.HideValue = true;
			if (const Pin* old = FindPinByName(pins, NewSocketPin)) extra.PinID = old->PinID;
			rebuilt.push_back(extra);
			pins = std::move(rebuilt);
		}
	}

	// ============================================================================
	// COPY (deep, groups included)
	// ============================================================================

	NodeGraph::NodeGraph(const NodeGraph& other) { *this = other; }

	NodeGraph& NodeGraph::operator=(const NodeGraph& other) {
		if (this == &other) return *this;
		m_Registry = other.m_Registry;
		m_Nodes = other.m_Nodes;
		m_Links = other.m_Links;
		m_InterfaceInputs = other.m_InterfaceInputs;
		m_InterfaceOutputs = other.m_InterfaceOutputs;
		m_Version = other.m_Version;
		auto counter = std::make_shared<ID>(other.m_Counter ? *other.m_Counter : 1);
		for (auto& n : m_Nodes)
			if (n.Subgraph) n.Subgraph = std::make_shared<NodeGraph>(*n.Subgraph);
		SetCounterDeep(counter);
		return *this;
	}

	void NodeGraph::SetCounterDeep(const std::shared_ptr<ID>& counter) {
		m_Counter = counter;
		for (auto& n : m_Nodes) if (n.Subgraph) n.Subgraph->SetCounterDeep(counter);
	}

	// ============================================================================
	// GRAPH
	// ============================================================================

	void NodeGraph::AssignPinIDs(Node& node) {
		for (auto& p : node.Inputs) { p.Kind = PinKind::Input; if (p.PinID == 0) p.PinID = NextID(); }
		for (auto& p : node.Outputs) { p.Kind = PinKind::Output; if (p.PinID == 0) p.PinID = NextID(); }
	}

	Node* NodeGraph::AddNode(const std::string& typeName, const glm::vec2& position) {
		const NodeTypeInfo* info = GetTypeInfo(typeName);
		if (!info) return nullptr;
		if (info->Unique && FindNodeByType(typeName)) return FindNodeByType(typeName);

		Node node;
		node.NodeID = NextID();
		node.TypeName = typeName;
		node.Title = info->Title;
		node.Position = position;
		if (info->Build) info->Build(node);
		BuildBuiltinPins(node);
		if (typeName == GroupType) {
			node.Subgraph = CreateSubgraph();
			SyncGroupNode(node);
		}
		AssignPinIDs(node);
		m_Nodes.push_back(std::move(node));
		Touch();
		return &m_Nodes.back();
	}

	bool NodeGraph::RemoveNode(ID nodeID) {
		auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(), [&](const Node& n) { return n.NodeID == nodeID; });
		if (it == m_Nodes.end()) return false;
		const NodeTypeInfo* info = GetTypeInfo(it->TypeName);
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

	Node* NodeGraph::DuplicateNode(ID nodeID, const glm::vec2& offset) {
		const Node* src = FindNode(nodeID);
		if (!src) return nullptr;
		const NodeTypeInfo* info = GetTypeInfo(src->TypeName);
		if (info && info->Unique) return nullptr;
		Node copy = *src;
		copy.NodeID = NextID();
		copy.Position += offset;
		for (auto& p : copy.Inputs) p.PinID = 0;
		for (auto& p : copy.Outputs) p.PinID = 0;
		AssignPinIDs(copy);
		if (copy.Subgraph) {
			// Fresh IDs for everything inside the copied group (IDs are unique in the whole tree)
			YAML::Emitter out;
			copy.Subgraph->Serialize(out);
			auto sub = std::make_shared<NodeGraph>(m_Registry);
			sub->m_Counter = m_Counter;
			const YAML::Node data = YAML::Load(out.c_str());
			sub->DeserializeInto(data);
			// Remap every ID of the loaded copy
			std::unordered_map<ID, ID> remap;
			auto fresh = [&](ID old) { auto [it, inserted] = remap.try_emplace(old, 0); if (inserted) it->second = NextID(); return it->second; };
			std::function<void(NodeGraph&)> reid = [&](NodeGraph& g) {
				for (auto& n : g.m_Nodes) {
					n.NodeID = fresh(n.NodeID);
					for (auto& p : n.Inputs) p.PinID = fresh(p.PinID);
					for (auto& p : n.Outputs) p.PinID = fresh(p.PinID);
					if (n.Subgraph) reid(*n.Subgraph);
				}
				for (auto& l : g.m_Links) { l.LinkID = fresh(l.LinkID); l.FromPin = fresh(l.FromPin); l.ToPin = fresh(l.ToPin); }
			};
			reid(*sub);
			copy.Subgraph = sub;
		}
		m_Nodes.push_back(std::move(copy));
		Touch();
		return &m_Nodes.back();
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

	Node* NodeGraph::FindNodeDeep(ID nodeID) {
		if (Node* n = FindNode(nodeID)) return n;
		for (auto& n : m_Nodes) if (n.Subgraph) if (Node* r = n.Subgraph->FindNodeDeep(nodeID)) return r;
		return nullptr;
	}
	const Node* NodeGraph::FindNodeDeep(ID nodeID) const { return const_cast<NodeGraph*>(this)->FindNodeDeep(nodeID); }

	Pin* NodeGraph::FindPinDeep(ID pinID) {
		if (Pin* p = FindPin(pinID)) return p;
		for (auto& n : m_Nodes) if (n.Subgraph) if (Pin* r = n.Subgraph->FindPinDeep(pinID)) return r;
		return nullptr;
	}
	const Pin* NodeGraph::FindPinDeep(ID pinID) const { return const_cast<NodeGraph*>(this)->FindPinDeep(pinID); }

	bool NodeGraph::CanConnect(ID a, ID b) const {
		const Pin* pa = FindPin(a);
		const Pin* pb = FindPin(b);
		if (!pa || !pb || pa->Kind == pb->Kind) return false;
		const Pin* out = pa->Kind == PinKind::Output ? pa : pb;
		const Pin* in = pa->Kind == PinKind::Input ? pa : pb;
		const Node* outNode = FindPinOwner(out->PinID);
		const Node* inNode = FindPinOwner(in->PinID);
		if (outNode == inNode) return false;
		// Virtual "new socket" pins only link to real sockets
		if (out->Name == NewSocketPin && in->Name == NewSocketPin) return false;
		// A reroute accepts anything (it takes the type of what feeds it)
		if (inNode && inNode->IsReroute()) return true;
		return ArePinTypesCompatible(out->Type, in->Type);
	}

	const Link* NodeGraph::Connect(ID a, ID b) {
		if (!CanConnect(a, b)) return nullptr;
		const Pin* pa = FindPin(a);
		ID from = pa->Kind == PinKind::Output ? a : b;
		ID to = pa->Kind == PinKind::Output ? b : a;

		// Group Input / Output: dropping on the empty socket adds a socket to the group interface
		{
			const Pin* fromPin = FindPin(from);
			const Pin* toPin = FindPin(to);
			const Node* fromNode = FindPinOwner(from);
			const Node* toNode = FindPinOwner(to);
			if (fromPin && fromPin->Name == NewSocketPin && fromNode && fromNode->TypeName == GroupInputType) {
				const std::string name = AddInterfaceSocket(true, toPin->DisplayName(), toPin->Type == PinType::Any ? PinType::Float : toPin->Type, toPin->DefaultValue, toPin->Range);
				from = FindNodeByType(GroupInputType)->FindOutput(name)->PinID;
			}
			else if (toPin && toPin->Name == NewSocketPin && toNode && toNode->TypeName == GroupOutputType) {
				const std::string name = AddInterfaceSocket(false, fromPin->DisplayName(), fromPin->Type == PinType::Any ? PinType::Float : fromPin->Type);
				to = FindNodeByType(GroupOutputType)->FindInput(name)->PinID;
			}
		}

		// One link per input
		m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return l.ToPin == to; }), m_Links.end());
		m_Links.push_back({ NextID(), from, to });

		// Reroutes take the type of their source (link colors, compatibility further down)
		if (Node* target = FindPinOwner(to); target && target->IsReroute()) {
			const Pin* src = FindPin(from);
			if (src && src->Type != PinType::Any) {
				target->Inputs[0].Type = src->Type;
				target->Outputs[0].Type = src->Type;
			}
		}
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
		for (int guard = 0; link && guard < 256; guard++) {
			const Node* src = FindPinOwner(link->FromPin);
			if (!src) break;
			if (!src->IsReroute()) return { src, FindPin(link->FromPin) };
			link = GetLinkInto(src->Inputs[0].PinID);
		}
		return { nullptr, nullptr };
	}

	std::vector<std::pair<const Node*, const Pin*>> NodeGraph::GetTargets(ID outputPin) const {
		std::vector<std::pair<const Node*, const Pin*>> result;
		std::vector<ID> stack{ outputPin };
		std::unordered_set<ID> seen;
		while (!stack.empty()) {
			const ID pin = stack.back();
			stack.pop_back();
			if (!seen.insert(pin).second) continue;
			for (const auto& l : m_Links) {
				if (l.FromPin != pin) continue;
				const Node* target = FindPinOwner(l.ToPin);
				if (!target) continue;
				if (target->IsReroute()) stack.push_back(target->Outputs[0].PinID);
				else result.push_back({ target, FindPin(l.ToPin) });
			}
		}
		return result;
	}

	// ============================================================================
	// GROUPS
	// ============================================================================

	std::string NodeGraph::AddInterfaceSocket(bool input, const std::string& name, PinType type, const glm::vec4& def, const glm::vec2& range) {
		auto& sockets = input ? m_InterfaceInputs : m_InterfaceOutputs;
		std::string base = name.empty() || name == NewSocketPin || name == " " ? (input ? "Input" : "Output") : name;
		std::string unique = base;
		for (int i = 1; std::any_of(sockets.begin(), sockets.end(), [&](const InterfaceSocket& s) { return s.Name == unique; }); i++)
			unique = base + " " + std::to_string(i);
		sockets.push_back({ unique, type, def, range });
		SyncInterfaceNodes();
		Touch();
		return unique;
	}

	void NodeGraph::RemoveInterfaceSocket(bool input, size_t index) {
		auto& sockets = input ? m_InterfaceInputs : m_InterfaceOutputs;
		if (index >= sockets.size()) return;
		const std::string name = sockets[index].Name;
		sockets.erase(sockets.begin() + index);
		if (Node* io = FindNodeByType(input ? GroupInputType : GroupOutputType)) {
			const Pin* pin = input ? io->FindOutput(name) : io->FindInput(name);
			if (pin) {
				const ID id = pin->PinID;
				m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return l.FromPin == id || l.ToPin == id; }), m_Links.end());
			}
		}
		SyncInterfaceNodes();
		Touch();
	}

	void NodeGraph::SyncInterfaceNodes() {
		for (auto& n : m_Nodes) {
			if (n.TypeName != GroupInputType && n.TypeName != GroupOutputType) continue;
			BuildBuiltinPins(n);
			AssignPinIDs(n);
		}
		// Links to sockets that no longer exist
		m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return !FindPin(l.FromPin) || !FindPin(l.ToPin); }), m_Links.end());
	}

	void NodeGraph::SyncGroupNode(Node& group) {
		if (!group.Subgraph) return;
		auto rebuild = [&](std::vector<Pin>& pins, const std::vector<InterfaceSocket>& sockets, bool inputs) {
			std::vector<Pin> result;
			auto inInterface = [&](const std::string& name) {
				return std::any_of(sockets.begin(), sockets.end(), [&](const InterfaceSocket& s) { return s.Name == name; });
			};
			for (size_t i = 0; i < sockets.size(); i++) {
				const InterfaceSocket& s = sockets[i];
				Pin p;
				p.Name = s.Name;
				p.Type = s.Type;
				p.Range = s.Range;
				p.DefaultValue = s.Default;
				p.Kind = inputs ? PinKind::Input : PinKind::Output;
				const Pin* old = FindPinByName(pins, s.Name);
				// Renamed socket: same position, old name gone -> keep its ID (and its links)
				if (!old && i < pins.size() && !inInterface(pins[i].Name)) old = &pins[i];
				if (old) {
					p.PinID = old->PinID;
					if (inputs && old->Type == s.Type) p.DefaultValue = old->DefaultValue;
				}
				result.push_back(p);
			}
			pins = std::move(result);
		};
		rebuild(group.Inputs, group.Subgraph->m_InterfaceInputs, true);
		rebuild(group.Outputs, group.Subgraph->m_InterfaceOutputs, false);
		AssignPinIDs(group);
		m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) { return !FindPin(l.FromPin) || !FindPin(l.ToPin); }), m_Links.end());
	}

	void NodeGraph::SyncGroupsDeep() {
		for (auto& n : m_Nodes) {
			if (!n.Subgraph) continue;
			n.Subgraph->SyncGroupsDeep();
			SyncGroupNode(n);
		}
	}

	std::shared_ptr<NodeGraph> NodeGraph::CreateSubgraph() {
		auto sub = std::make_shared<NodeGraph>(m_Registry);
		sub->m_Counter = m_Counter;
		sub->AddNode(GroupInputType, { -300.0f, 0.0f });
		sub->AddNode(GroupOutputType, { 300.0f, 0.0f });
		return sub;
	}

	ID NodeGraph::MakeGroup(const std::vector<ID>& nodeIDs) {
		// Nodes that can be grouped (not unique nodes such as the material output)
		std::unordered_set<ID> selected;
		for (ID id : nodeIDs) {
			const Node* n = FindNode(id);
			const NodeTypeInfo* info = n ? GetTypeInfo(n->TypeName) : nullptr;
			if (n && !(info && info->Unique)) selected.insert(id);
		}
		if (selected.empty()) return 0;

		auto ownerSelected = [&](ID pin) { const Node* o = FindPinOwner(pin); return o && selected.count(o->NodeID); };

		glm::vec2 minP(1e9f), maxP(-1e9f), center(0.0f);
		for (ID id : selected) {
			const Node* n = FindNode(id);
			minP = glm::min(minP, n->Position);
			maxP = glm::max(maxP, n->Position);
			center += n->Position;
		}
		center /= static_cast<float>(selected.size());

		Node* groupNode = AddNode(GroupType, center);
		const ID groupID = groupNode->NodeID;
		NodeGraph& sub = *groupNode->Subgraph;
		sub.FindNodeByType(GroupInputType)->Position = { minP.x - 260.0f, center.y };
		sub.FindNodeByType(GroupOutputType)->Position = { maxP.x + 260.0f, center.y };

		// Move the nodes (IDs are kept: the ID space is shared)
		for (auto it = m_Nodes.begin(); it != m_Nodes.end();) {
			if (selected.count(it->NodeID)) { sub.m_Nodes.push_back(std::move(*it)); it = m_Nodes.erase(it); }
			else ++it;
		}

		// Sort links: internal ones move, crossing ones become sockets
		std::vector<Link> keep, incoming, outgoing;
		for (const auto& l : m_Links) {
			const bool fromIn = sub.FindPin(l.FromPin) != nullptr;
			const bool toIn = sub.FindPin(l.ToPin) != nullptr;
			if (fromIn && toIn) sub.m_Links.push_back(l);
			else if (!fromIn && toIn) incoming.push_back(l);
			else if (fromIn && !toIn) outgoing.push_back(l);
			else keep.push_back(l);
		}
		m_Links = keep;
		(void)ownerSelected;

		// Incoming: one group input per outside source pin
		std::unordered_map<ID, std::string> inputForSource;
		std::vector<std::pair<ID, std::string>> parentInputs;   // outside source -> group socket
		for (const auto& l : incoming) {
			const Pin* target = sub.FindPin(l.ToPin);
			std::string name;
			if (auto it = inputForSource.find(l.FromPin); it != inputForSource.end()) name = it->second;
			else {
				name = sub.AddInterfaceSocket(true, target->DisplayName(), target->Type == PinType::Any ? PinType::Float : target->Type, target->DefaultValue, target->Range);
				inputForSource[l.FromPin] = name;
				parentInputs.push_back({ l.FromPin, name });
			}
			sub.m_Links.push_back({ NextID(), sub.FindNodeByType(GroupInputType)->FindOutput(name)->PinID, l.ToPin });
		}
		// Outgoing: one group output per inside source pin
		std::unordered_map<ID, std::string> outputForSource;
		std::vector<std::pair<std::string, ID>> parentOutputs;  // group socket -> outside target
		for (const auto& l : outgoing) {
			const Pin* source = sub.FindPin(l.FromPin);
			std::string name;
			if (auto it = outputForSource.find(l.FromPin); it != outputForSource.end()) name = it->second;
			else {
				name = sub.AddInterfaceSocket(false, source->DisplayName(), source->Type == PinType::Any ? PinType::Float : source->Type);
				outputForSource[l.FromPin] = name;
				sub.m_Links.push_back({ NextID(), l.FromPin, sub.FindNodeByType(GroupOutputType)->FindInput(name)->PinID });
			}
			parentOutputs.push_back({ name, l.ToPin });
		}

		Node* group = FindNode(groupID);
		SyncGroupNode(*group);
		for (const auto& [src, name] : parentInputs) m_Links.push_back({ NextID(), src, group->FindInput(name)->PinID });
		for (const auto& [name, dst] : parentOutputs) m_Links.push_back({ NextID(), group->FindOutput(name)->PinID, dst });
		Touch();
		return groupID;
	}

	bool NodeGraph::Ungroup(ID groupNodeID) {
		Node* group = FindNode(groupNodeID);
		if (!group || !group->Subgraph) return false;
		NodeGraph& sub = *group->Subgraph;
		const Node* gin = sub.FindNodeByType(GroupInputType);
		const Node* gout = sub.FindNodeByType(GroupOutputType);
		const glm::vec2 offset = group->Position;

		// Group socket -> outside source / targets
		std::unordered_map<std::string, ID> sourceOfInput;
		std::unordered_map<std::string, glm::vec4> defaultOfInput;
		for (const auto& in : group->Inputs) {
			defaultOfInput[in.Name] = in.DefaultValue;
			if (const Link* l = GetLinkInto(in.PinID)) sourceOfInput[in.Name] = l->FromPin;
		}
		std::unordered_map<std::string, std::vector<ID>> targetsOfOutput;
		for (const auto& out : group->Outputs)
			for (const auto& l : m_Links) if (l.FromPin == out.PinID) targetsOfOutput[out.Name].push_back(l.ToPin);

		const Node groupCopy = *group;
		RemoveNode(groupNodeID);

		std::vector<Link> newLinks;
		for (const auto& l : sub.m_Links) {
			const Node* from = sub.FindPinOwner(l.FromPin);
			const Node* to = sub.FindPinOwner(l.ToPin);
			const bool fromIO = from == gin, toIO = to == gout;
			if (!fromIO && !toIO) { newLinks.push_back(l); continue; }
			if (fromIO && !toIO) {
				const std::string name = sub.FindPin(l.FromPin)->Name;
				if (auto it = sourceOfInput.find(name); it != sourceOfInput.end()) newLinks.push_back({ NextID(), it->second, l.ToPin });
				else if (Pin* p = sub.FindPin(l.ToPin)) p->DefaultValue = defaultOfInput[name];
			}
			else if (!fromIO && toIO) {
				const std::string name = sub.FindPin(l.ToPin)->Name;
				for (ID dst : targetsOfOutput[name]) newLinks.push_back({ NextID(), l.FromPin, dst });
			}
		}
		// Group input wired straight to group output
		for (const auto& l : sub.m_Links) {
			if (sub.FindPinOwner(l.FromPin) != gin || sub.FindPinOwner(l.ToPin) != gout) continue;
			auto src = sourceOfInput.find(sub.FindPin(l.FromPin)->Name);
			if (src == sourceOfInput.end()) continue;
			for (ID dst : targetsOfOutput[sub.FindPin(l.ToPin)->Name]) newLinks.push_back({ NextID(), src->second, dst });
		}

		glm::vec2 center(0.0f);
		int count = 0;
		for (const auto& n : sub.m_Nodes) if (&n != gin && &n != gout) { center += n.Position; count++; }
		if (count) center /= static_cast<float>(count);
		for (auto& n : sub.m_Nodes) {
			if (n.TypeName == GroupInputType || n.TypeName == GroupOutputType) continue;
			Node moved = std::move(n);
			moved.Position = moved.Position - center + offset;
			m_Nodes.push_back(std::move(moved));
		}
		for (const auto& l : newLinks) {
			// One link per input
			m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& x) { return x.ToPin == l.ToPin; }), m_Links.end());
			if (FindPin(l.FromPin) && FindPin(l.ToPin)) m_Links.push_back(l);
		}
		(void)groupCopy;
		Touch();
		return true;
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
		out << YAML::Key << "NextID" << YAML::Value << (m_Counter ? *m_Counter : 1);

		if (!m_InterfaceInputs.empty() || !m_InterfaceOutputs.empty()) {
			out << YAML::Key << "Interface" << YAML::Value << YAML::BeginMap;
			auto emitSockets = [&](const char* key, const std::vector<InterfaceSocket>& sockets) {
				out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
				for (const auto& s : sockets) {
					out << YAML::Flow << YAML::BeginMap;
					out << YAML::Key << "Name" << YAML::Value << s.Name;
					out << YAML::Key << "Type" << YAML::Value << PinTypeToString(s.Type);
					out << YAML::Key << "Default" << YAML::Value; EmitVec4(out, s.Default);
					out << YAML::Key << "Range" << YAML::Value << YAML::Flow << YAML::BeginSeq << s.Range.x << s.Range.y << YAML::EndSeq;
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			};
			emitSockets("Inputs", m_InterfaceInputs);
			emitSockets("Outputs", m_InterfaceOutputs);
			out << YAML::EndMap;
		}

		out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
		for (const auto& n : m_Nodes) {
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << n.NodeID;
			out << YAML::Key << "Type" << YAML::Value << n.TypeName;
			if (n.IsGroup() && !n.Title.empty()) out << YAML::Key << "Title" << YAML::Value << n.Title;
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
					if (n.IsReroute()) out << YAML::Key << "Type" << YAML::Value << PinTypeToString(p.Type);
					if (p.Kind == PinKind::Input) { out << YAML::Key << "Default" << YAML::Value; EmitVec4(out, p.DefaultValue); }
					if (p.Kind == PinKind::Input && !p.StringValue.empty()) out << YAML::Key << "String" << YAML::Value << p.StringValue;
					out << YAML::EndMap;
				}
			};
			emitPins(n.Inputs, "In");
			emitPins(n.Outputs, "Out");
			out << YAML::EndSeq;
			if (n.Subgraph) {
				out << YAML::Key << "Subgraph" << YAML::Value;
				n.Subgraph->Serialize(out);
			}
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
		m_Counter = std::make_shared<ID>(data["NextID"].as<uint64_t>(1));
		if (!DeserializeInto(data)) return false;
		// Keep the counter above every ID in use (nested groups included)
		*m_Counter = std::max(*m_Counter, MaxIDDeep() + 1);
		SyncGroupsDeep();
		Touch();
		return true;
	}

	ID NodeGraph::MaxIDDeep() const {
		ID maxID = 0;
		for (const auto& n : m_Nodes) {
			maxID = std::max(maxID, n.NodeID);
			for (const auto& p : n.Inputs) maxID = std::max(maxID, p.PinID);
			for (const auto& p : n.Outputs) maxID = std::max(maxID, p.PinID);
			if (n.Subgraph) maxID = std::max(maxID, n.Subgraph->MaxIDDeep());
		}
		for (const auto& l : m_Links) maxID = std::max(maxID, l.LinkID);
		return maxID;
	}

	bool NodeGraph::DeserializeInto(const YAML::Node& data) {
		if (!data || !data.IsMap()) return false;
		m_Nodes.clear();
		m_Links.clear();
		m_InterfaceInputs.clear();
		m_InterfaceOutputs.clear();

		if (auto iface = data["Interface"]; iface && iface.IsMap()) {
			auto readSockets = [&](const YAML::Node& seq, std::vector<InterfaceSocket>& sockets) {
				if (!seq || !seq.IsSequence()) return;
				for (const auto& s : seq) {
					InterfaceSocket socket;
					socket.Name = s["Name"].as<std::string>("");
					socket.Type = PinTypeFromString(s["Type"].as<std::string>("Float"));
					socket.Default = ReadVec4(s["Default"], glm::vec4(0.0f));
					if (auto r = s["Range"]; r && r.IsSequence() && r.size() >= 2) socket.Range = { r[0].as<float>(), r[1].as<float>() };
					sockets.push_back(socket);
				}
			};
			readSockets(iface["Inputs"], m_InterfaceInputs);
			readSockets(iface["Outputs"], m_InterfaceOutputs);
		}

		if (auto nodes = data["Nodes"]; nodes && nodes.IsSequence()) {
			for (const auto& nd : nodes) {
				Node node;
				node.NodeID = nd["ID"].as<uint64_t>(0);
				node.TypeName = nd["Type"].as<std::string>("");
				if (auto pos = nd["Position"]; pos && pos.size() >= 2) node.Position = { pos[0].as<float>(), pos[1].as<float>() };

				// Rebuild pins from the registry so newer node versions gain their pins
				const NodeTypeInfo* info = GetTypeInfo(node.TypeName);
				if (!info) continue;   // unknown node type: dropped (its links are dropped too)
				node.Title = info->Title;
				if (node.TypeName == GroupType && nd["Title"]) node.Title = nd["Title"].as<std::string>(info->Title);
				if (info->Build) info->Build(node);
				BuildBuiltinPins(node);
				node.Value = ReadVec4(nd["Value"], node.Value);
				if (auto props = nd["Properties"]; props && props.IsMap())
					for (auto it = props.begin(); it != props.end(); ++it)
						node.Properties[it->first.as<std::string>()] = it->second.as<std::string>("");

				if (node.TypeName == GroupType) {
					node.Subgraph = std::make_shared<NodeGraph>(m_Registry);
					node.Subgraph->m_Counter = m_Counter;
					if (auto subData = nd["Subgraph"]) node.Subgraph->DeserializeInto(subData);
					SyncGroupNode(node);
				}

				// Restore pin IDs/defaults by name
				if (auto pins = nd["Pins"]; pins && pins.IsSequence()) {
					for (const auto& pd : pins) {
						std::string name = pd["Name"].as<std::string>("");
						bool isInput = pd["Kind"].as<std::string>("In") == "In";
						Pin* pin = isInput ? node.FindInput(name) : node.FindOutput(name);
						if (!pin) continue;
						pin->PinID = pd["ID"].as<uint64_t>(0);
						if (node.IsReroute() && pd["Type"]) pin->Type = PinTypeFromString(pd["Type"].as<std::string>("Any"));
						if (isInput) pin->DefaultValue = ReadVec4(pd["Default"], pin->DefaultValue);
						if (isInput && pd["String"]) pin->StringValue = pd["String"].as<std::string>("");
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
		return true;
	}

} // namespace Lunex::Graph
