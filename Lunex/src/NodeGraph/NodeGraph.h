#pragma once

/**
 * @file NodeGraph.h
 * @brief Core node graph data structure
 * 
 * The NodeGraph owns all nodes, pins, and links. It provides:
 * - Node/link CRUD operations
 * - Topological sorting for evaluation order
 * - Connection validation
 * - Serialization interface
 * 
 * This is domain-agnostic. Each domain (Shader, Animation, etc.)
 * uses a NodeGraph instance with domain-specific NodeFactory.
 */

#include "Node.h"
#include "Link.h"

#include <algorithm>

namespace Lunex::NodeGraph {

	class NodeGraph {
	public:
		NodeGraph() = default;
		NodeGraph(const std::string& name, GraphDomain domain)
			: m_Name(name), m_Domain(domain) {}
		~NodeGraph() = default;

		// ========== IDENTIFICATION ==========

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }
		
		GraphDomain GetDomain() const { return m_Domain; }
		void SetDomain(GraphDomain domain) { m_Domain = domain; }

		UUID GetID() const { return m_ID; }
		void SetID(UUID id) { m_ID = id; }

		// ========== NODE MANAGEMENT ==========

		NodeID AddNode(Ref<Node> node) {
			if (!node) return InvalidNodeID;
			if (node->ID == InvalidNodeID)
				node->ID = AllocateNodeID();
			
			// Ensure pins reference this node
			for (auto& pin : node->Inputs)
				pin.OwnerNodeID = node->ID;
			for (auto& pin : node->Outputs)
				pin.OwnerNodeID = node->ID;

			m_Nodes[node->ID] = node;
			m_IsDirty = true;
			return node->ID;
		}

		bool RemoveNode(NodeID id) {
			auto it = m_Nodes.find(id);
			if (it == m_Nodes.end()) return false;

			// Remove all links connected to this node
			RemoveLinksForNode(id);
			m_Nodes.erase(it);
			m_IsDirty = true;
			return true;
		}

		Node* GetNode(NodeID id) {
			auto it = m_Nodes.find(id);
			return it != m_Nodes.end() ? it->second.get() : nullptr;
		}

		const Node* GetNode(NodeID id) const {
			auto it = m_Nodes.find(id);
			return it != m_Nodes.end() ? it->second.get() : nullptr;
		}

		Ref<Node> GetNodeRef(NodeID id) {
			auto it = m_Nodes.find(id);
			return it != m_Nodes.end() ? it->second : nullptr;
		}

		const std::unordered_map<NodeID, Ref<Node>>& GetNodes() const { return m_Nodes; }
		size_t GetNodeCount() const { return m_Nodes.size(); }

		// ========== LINK MANAGEMENT ==========

		LinkID AddLink(PinID startPin, PinID endPin) {
			// Validate connection
			Pin* srcPin = FindPin(startPin);
			Pin* dstPin = FindPin(endPin);
			if (!srcPin || !dstPin) return InvalidLinkID;
			
			// Ensure start is output, end is input
			PinID outPin = startPin;
			PinID inPin = endPin;
			if (srcPin->IsInput() && dstPin->IsOutput()) {
				std::swap(outPin, inPin);
				std::swap(srcPin, dstPin);
			}
			
			if (!srcPin->IsOutput() || !dstPin->IsInput()) return InvalidLinkID;
			if (!srcPin->CanConnectTo(*dstPin)) return InvalidLinkID;

			// Remove existing link to the input pin (inputs can only have one connection)
			RemoveLinksForPin(inPin, PinDirection::Input);

			LinkID id = AllocateLinkID();
			Link link(id, outPin, inPin);
			link.Color = Pin::GetTypeColor(srcPin->DataType);
			m_Links[id] = link;

			srcPin->IsConnected = true;
			dstPin->IsConnected = true;
			
			m_IsDirty = true;
			return id;
		}

		bool RemoveLink(LinkID id) {
			auto it = m_Links.find(id);
			if (it == m_Links.end()) return false;

			const auto& link = it->second;
			UpdatePinConnectionState(link.StartPinID);
			UpdatePinConnectionState(link.EndPinID);
			
			m_Links.erase(it);
			m_IsDirty = true;
			return true;
		}

		const Link* GetLink(LinkID id) const {
			auto it = m_Links.find(id);
			return it != m_Links.end() ? &it->second : nullptr;
		}

		const std::unordered_map<LinkID, Link>& GetLinks() const { return m_Links; }
		size_t GetLinkCount() const { return m_Links.size(); }

		// ========== PIN QUERIES ==========

		Pin* FindPin(PinID pinID) {
			for (auto& [nodeID, node] : m_Nodes) {
				if (Pin* pin = node->FindPin(pinID))
					return pin;
			}
			return nullptr;
		}

		const Pin* FindPin(PinID pinID) const {
			for (const auto& [nodeID, node] : m_Nodes) {
				if (const Pin* pin = node->FindPin(pinID))
					return pin;
			}
			return nullptr;
		}

		Node* FindPinOwner(PinID pinID) {
			for (auto& [nodeID, node] : m_Nodes) {
				if (node->FindPin(pinID))
					return node.get();
			}
			return nullptr;
		}

		// Get all links connected to a specific pin
		std::vector<const Link*> GetLinksForPin(PinID pinID) const {
			std::vector<const Link*> result;
			for (const auto& [id, link] : m_Links) {
				if (link.StartPinID == pinID || link.EndPinID == pinID)
					result.push_back(&link);
			}
			return result;
		}

		// Get the pin connected to an input pin (follow the link upstream)
		Pin* GetConnectedOutputPin(PinID inputPinID) {
			for (const auto& [id, link] : m_Links) {
				if (link.EndPinID == inputPinID)
					return FindPin(link.StartPinID);
			}
			return nullptr;
		}

		// ========== TOPOLOGICAL SORT ==========

		std::vector<NodeID> GetTopologicalOrder() const {
			std::vector<NodeID> order;
			std::unordered_map<NodeID, int> inDegree;
			std::unordered_map<NodeID, std::vector<NodeID>> adjacency;

			// Initialize
			for (const auto& [id, node] : m_Nodes) {
				inDegree[id] = 0;
			}

			// Build adjacency from links
			for (const auto& [linkID, link] : m_Links) {
				const Pin* startPin = FindPin(link.StartPinID);
				const Pin* endPin = FindPin(link.EndPinID);
				if (startPin && endPin) {
					NodeID fromNode = startPin->OwnerNodeID;
					NodeID toNode = endPin->OwnerNodeID;
					adjacency[fromNode].push_back(toNode);
					inDegree[toNode]++;
				}
			}

			// Kahn's algorithm
			std::vector<NodeID> queue;
			for (const auto& [id, degree] : inDegree) {
				if (degree == 0)
					queue.push_back(id);
			}

			while (!queue.empty()) {
				NodeID current = queue.back();
				queue.pop_back();
				order.push_back(current);

				if (adjacency.find(current) != adjacency.end()) {
					for (NodeID neighbor : adjacency[current]) {
						inDegree[neighbor]--;
						if (inDegree[neighbor] == 0)
							queue.push_back(neighbor);
					}
				}
			}

			return order;
		}

		bool HasCycle() const {
			return GetTopologicalOrder().size() != m_Nodes.size();
		}

		// ========== VALIDATION ==========

		bool Validate() {
			bool valid = true;
			for (auto& [id, node] : m_Nodes) {
				if (!node->Validate()) {
					valid = false;
				}
			}
			if (HasCycle()) {
				valid = false;
			}
			return valid;
		}

		// ========== STATE ==========

		bool IsDirty() const { return m_IsDirty; }
		void ClearDirty() { m_IsDirty = false; }
		void MarkDirty() { m_IsDirty = true; }

		// ========== CLEAR ==========

		void Clear() {
			m_Nodes.clear();
			m_Links.clear();
			m_NextNodeID = 1;
			m_NextPinID = 1;
			m_NextLinkID = 1;
			m_IsDirty = true;
		}

		// ========== ID ALLOCATION ==========

		NodeID AllocateNodeID() { return m_NextNodeID++; }
		PinID AllocatePinID() { return m_NextPinID++; }
		LinkID AllocateLinkID() { return m_NextLinkID++; }

		void SetNextNodeID(NodeID id) { m_NextNodeID = id; }
		void SetNextPinID(PinID id) { m_NextPinID = id; }
		void SetNextLinkID(LinkID id) { m_NextLinkID = id; }

	private:
		void RemoveLinksForNode(NodeID nodeID) {
			auto node = GetNode(nodeID);
			if (!node) return;

			std::vector<LinkID> toRemove;
			for (const auto& [linkID, link] : m_Links) {
				const Pin* startPin = FindPin(link.StartPinID);
				const Pin* endPin = FindPin(link.EndPinID);
				if ((startPin && startPin->OwnerNodeID == nodeID) ||
					(endPin && endPin->OwnerNodeID == nodeID)) {
					toRemove.push_back(linkID);
				}
			}
			for (LinkID id : toRemove) {
				RemoveLink(id);
			}
		}

		void RemoveLinksForPin(PinID pinID, PinDirection direction) {
			std::vector<LinkID> toRemove;
			for (const auto& [linkID, link] : m_Links) {
				if (direction == PinDirection::Input && link.EndPinID == pinID)
					toRemove.push_back(linkID);
				else if (direction == PinDirection::Output && link.StartPinID == pinID)
					toRemove.push_back(linkID);
			}
			for (LinkID id : toRemove) {
				RemoveLink(id);
			}
		}

		void UpdatePinConnectionState(PinID pinID) {
			Pin* pin = FindPin(pinID);
			if (!pin) return;

			bool connected = false;
			for (const auto& [linkID, link] : m_Links) {
				if (link.StartPinID == pinID || link.EndPinID == pinID) {
					connected = true;
					break;
				}
			}
			pin->IsConnected = connected;
		}

	private:
		std::string m_Name = "Untitled";
		GraphDomain m_Domain = GraphDomain::None;
		UUID m_ID;

		std::unordered_map<NodeID, Ref<Node>> m_Nodes;
		std::unordered_map<LinkID, Link> m_Links;

		NodeID m_NextNodeID = 1;
		PinID m_NextPinID = 1;
		LinkID m_NextLinkID = 1;

		bool m_IsDirty = false;
	};

} // namespace Lunex::NodeGraph
