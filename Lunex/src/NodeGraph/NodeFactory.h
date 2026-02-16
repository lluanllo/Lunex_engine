#pragma once

/**
 * @file NodeFactory.h
 * @brief Factory for creating and registering node types
 * 
 * Each domain registers its node types through the NodeFactory.
 * This enables:
 * - Runtime node creation from type names (for deserialization)
 * - Node palette/menu generation in the editor
 * - Domain-specific node organization
 */

#include "Node.h"
#include "NodeGraph.h"

namespace Lunex::NodeGraph {

	struct NodeRegistration {
		std::string TypeName;
		std::string DisplayName;
		std::string Category;
		GraphDomain Domain = GraphDomain::None;
		uint32_t HeaderColor = PackColor(60, 60, 60);
		std::string Tooltip;
		std::function<Ref<Node>(NodeGraph& graph)> CreateFunc;
	};

	class NodeFactory {
	public:
		static NodeFactory& Get() {
			static NodeFactory instance;
			return instance;
		}

		// ========== REGISTRATION ==========

		void Register(const NodeRegistration& reg) {
			m_Registry[reg.TypeName] = reg;
			m_DomainNodes[reg.Domain].push_back(reg.TypeName);
		}

		template<typename NodeType>
		void Register(const std::string& typeName, const std::string& displayName,
					  const std::string& category, GraphDomain domain,
					  uint32_t headerColor = PackColor(60, 60, 60),
					  const std::string& tooltip = "") {
			NodeRegistration reg;
			reg.TypeName = typeName;
			reg.DisplayName = displayName;
			reg.Category = category;
			reg.Domain = domain;
			reg.HeaderColor = headerColor;
			reg.Tooltip = tooltip;
			reg.CreateFunc = [=](NodeGraph& graph) -> Ref<Node> {
				auto node = CreateRef<NodeType>();
				node->ID = graph.AllocateNodeID();
				node->TypeName = typeName;
				node->DisplayName = displayName;
				node->Category = category;
				node->HeaderColor = headerColor;
				return node;
			};
			Register(reg);
		}

		// ========== CREATION ==========

		Ref<Node> CreateNode(const std::string& typeName, NodeGraph& graph) const {
			auto it = m_Registry.find(typeName);
			if (it == m_Registry.end()) return nullptr;
			return it->second.CreateFunc(graph);
		}

		// ========== QUERIES ==========

		bool HasType(const std::string& typeName) const {
			return m_Registry.find(typeName) != m_Registry.end();
		}

		const NodeRegistration* GetRegistration(const std::string& typeName) const {
			auto it = m_Registry.find(typeName);
			return it != m_Registry.end() ? &it->second : nullptr;
		}

		std::vector<const NodeRegistration*> GetNodesForDomain(GraphDomain domain) const {
			std::vector<const NodeRegistration*> result;
			auto it = m_DomainNodes.find(domain);
			if (it != m_DomainNodes.end()) {
				for (const auto& typeName : it->second) {
					auto regIt = m_Registry.find(typeName);
					if (regIt != m_Registry.end())
						result.push_back(&regIt->second);
				}
			}
			return result;
		}

		// Get all categories for a domain
		std::vector<std::string> GetCategoriesForDomain(GraphDomain domain) const {
			std::vector<std::string> categories;
			auto nodes = GetNodesForDomain(domain);
			for (const auto* reg : nodes) {
				if (std::find(categories.begin(), categories.end(), reg->Category) == categories.end())
					categories.push_back(reg->Category);
			}
			std::sort(categories.begin(), categories.end());
			return categories;
		}

		// Get all nodes in a specific category for a domain
		std::vector<const NodeRegistration*> GetNodesInCategory(GraphDomain domain, const std::string& category) const {
			std::vector<const NodeRegistration*> result;
			auto nodes = GetNodesForDomain(domain);
			for (const auto* reg : nodes) {
				if (reg->Category == category)
					result.push_back(reg);
			}
			return result;
		}

		const std::unordered_map<std::string, NodeRegistration>& GetAllRegistrations() const {
			return m_Registry;
		}

	private:
		NodeFactory() = default;

		std::unordered_map<std::string, NodeRegistration> m_Registry;
		std::unordered_map<GraphDomain, std::vector<std::string>> m_DomainNodes;
	};

	// ============================================================================
	// REGISTRATION MACRO
	// ============================================================================

	#define LNX_REGISTER_NODE(NodeType, TypeName, DisplayName, Category, Domain, Color) \
		namespace { \
			struct NodeType##_Registrar { \
				NodeType##_Registrar() { \
					::Lunex::NodeGraph::NodeFactory::Get().Register<NodeType>( \
						TypeName, DisplayName, Category, Domain, Color); \
				} \
			}; \
			static NodeType##_Registrar s_##NodeType##_registrar; \
		}

} // namespace Lunex::NodeGraph
