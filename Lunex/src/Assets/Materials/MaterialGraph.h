#pragma once

/**
 * @file MaterialGraph.h
 * @brief Node-based authoring for MaterialAsset (built on Graph::NodeGraph).
 *
 * Today the graph is "baked" into the regular PBR parameters of MaterialAsset, so it
 * renders through the existing deferred/forward shaders with zero runtime cost:
 *   Texture Sample.RGB  -> map slot           Color / Float     -> constant value
 *   Texture Sample.R/G/B/A -> map or packed-channel slot (ORM textures)
 *   Multiply(Texture, Color|Float) -> map + tint / multiplier
 * Anything the fixed shader can't express is reported as a warning. The next step
 * (GLSL code generation per graph) can reuse this same data model unchanged.
 */

#include "Graph/NodeGraph.h"

#include <string>
#include <vector>

namespace Lunex {

	class MaterialAsset;

	namespace MaterialGraph {

		// Node type names
		inline constexpr const char* OutputNode   = "Material.Output";
		inline constexpr const char* TextureNode  = "Material.TextureSample";
		inline constexpr const char* ColorNode    = "Material.Color";
		inline constexpr const char* FloatNode    = "Material.Float";
		inline constexpr const char* MultiplyNode = "Material.Multiply";

		const Graph::NodeRegistry& GetRegistry();

		// Build a graph that reproduces the material's current slider/texture setup
		std::shared_ptr<Graph::NodeGraph> CreateFromMaterial(const MaterialAsset& material);

		// Write the graph into the material's PBR parameters. Returns warnings.
		std::vector<std::string> Apply(const Graph::NodeGraph& graph, MaterialAsset& material);

	}

} // namespace Lunex
