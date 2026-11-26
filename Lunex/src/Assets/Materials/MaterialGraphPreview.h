#pragma once

/**
 * @file MaterialGraphPreview.h
 * @brief Node previews for the material node editor (like Blender's Node Wrangler / Node Preview).
 *
 * Every node of a graph is compiled into ONE shader (`vec4 lnx_preview(int target)`), so a
 * structural edit costs a single compile no matter how many nodes there are. Each node is then
 * drawn on a flat UV plane into its own tile of an atlas; the editor shows the tile in the node.
 * Tiles are only re-rendered when the graph changes (values included).
 */

#include "Core/Core.h"
#include "Graph/NodeGraph.h"
#include "MaterialGraphCompiler.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

namespace Lunex {

	class Framebuffer;
	class VertexArray;
	class VertexBuffer;

	namespace MaterialGraph {

		class NodePreviewRenderer {
		public:
			explicit NodePreviewRenderer(uint32_t tileSize = 128);

			// Re-renders the previews when the graph changed (call outside ImGui, e.g. OnUpdate)
			void Update(const Graph::NodeGraph& graph);
			// Atlas texture + UVs (ImGui order: uv0 = top-left) of a node's preview
			bool GetTile(Graph::ID node, uint32_t& textureID, glm::vec2& uv0, glm::vec2& uv1) const;
			void Invalidate() { m_Version = 0; }

		private:
			void Render();

			uint32_t m_TileSize;
			uint32_t m_Cols = 8, m_Rows = 0;
			Ref<Framebuffer> m_Atlas;
			Ref<VertexArray> m_Quad;
			Ref<VertexBuffer> m_QuadVB;
			Ref<Shader> m_Shader;
			Ref<UniformBuffer> m_ParamsUBO;
			CompiledGraph m_Compiled;
			std::vector<Ref<Texture2D>> m_Textures;
			std::vector<glm::vec4> m_Params;
			std::unordered_map<Graph::ID, int> m_Index;
			const Graph::NodeGraph* m_Graph = nullptr;
			uint64_t m_Version = 0;
			uint64_t m_CodeHash = 0;
			bool m_ShaderFailed = false;
		};

	}

} // namespace Lunex
