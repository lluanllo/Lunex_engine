#pragma once

/**
 * @file NodeGraphAsset.h
 * @brief Asset wrapper for node graphs
 * 
 * Integrates node graphs with the Lunex unified asset system.
 * Supports saving/loading as .lunodegraph files.
 */

#include "Assets/Core/Asset.h"
#include "NodeGraph/NodeGraphCore.h"
#include "NodeGraph/NodeGraphSerializer.h"

namespace Lunex {

	class NodeGraphAsset : public Asset {
	public:
		NodeGraphAsset()
			: Asset("New Node Graph") {
			m_Graph = CreateRef<NodeGraph::NodeGraph>("Untitled", NodeGraph::GraphDomain::None);
		}

		NodeGraphAsset(const std::string& name, NodeGraph::GraphDomain domain)
			: Asset(name) {
			m_Graph = CreateRef<NodeGraph::NodeGraph>(name, domain);
		}

		~NodeGraphAsset() override = default;

		// ========== ASSET INTERFACE ==========

		AssetType GetType() const override { return AssetType::None; } // TODO: Add NodeGraph asset type
		const char* GetTypeName() const override { return "NodeGraph"; }
		const char* GetExtension() const override { return ".lunodegraph"; }

		bool SaveToFile(const std::filesystem::path& path) override {
			if (!m_Graph) return false;
			bool success = NodeGraph::NodeGraphSerializer::SaveToFile(*m_Graph, path);
			if (success) {
				SetPath(path);
				ClearDirty();
			}
			return success;
		}

		static Ref<NodeGraphAsset> LoadFromFile(const std::filesystem::path& path) {
			auto asset = CreateRef<NodeGraphAsset>();
			if (NodeGraph::NodeGraphSerializer::LoadFromFile(*asset->m_Graph, path)) {
				asset->SetPath(path);
				asset->SetName(asset->m_Graph->GetName());
				asset->SetLoaded(true);
				return asset;
			}
			return nullptr;
		}

		// ========== GRAPH ACCESS ==========

		Ref<NodeGraph::NodeGraph> GetGraph() const { return m_Graph; }
		void SetGraph(Ref<NodeGraph::NodeGraph> graph) { m_Graph = graph; MarkDirty(); }

	private:
		Ref<NodeGraph::NodeGraph> m_Graph;
	};

} // namespace Lunex
