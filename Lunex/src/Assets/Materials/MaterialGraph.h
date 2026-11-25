#pragma once

/**
 * @file MaterialGraph.h
 * @brief Node-based authoring for MaterialAsset (built on Graph::NodeGraph).
 *
 * The node set follows Blender's shader editor (Principled BSDF, Mix, Math, Vector
 * Math, Map Range, Color Ramp, Separate/Combine, Image/Noise/Voronoi/Wave/Checker
 * textures, Normal Map, Bump, Mapping, Fresnel, Layer Weight...).
 *
 * A graph is compiled to GLSL (MaterialGraphCompiler) and rendered with its own
 * geometry-pass / forward shader. Constant values live in a parameter UBO so editing a
 * slider never recompiles; only structural edits (links, nodes, operations) do.
 *
 * Apply() additionally "bakes" what the fixed PBR shader can express into the regular
 * MaterialAsset parameters: used as a fallback when the graph shader fails to compile
 * and by tools that only read the classic parameters.
 */

#include "Graph/NodeGraph.h"

#include <string>
#include <vector>

namespace Lunex {

	class MaterialAsset;

	namespace MaterialGraph {

		// ---- Output / shaders ----
		inline constexpr const char* MaterialOutput = "Mat.Output";
		inline constexpr const char* PrincipledBSDF = "Mat.PrincipledBSDF";
		inline constexpr const char* EmissionShader = "Mat.Emission";
		inline constexpr const char* MixShader      = "Mat.MixShader";
		inline constexpr const char* AddShader      = "Mat.AddShader";

		// ---- Input ----
		inline constexpr const char* TexCoordNode   = "Mat.TexCoord";
		inline constexpr const char* GeometryNode   = "Mat.Geometry";
		inline constexpr const char* ValueNode      = "Mat.Value";
		inline constexpr const char* RGBNode        = "Mat.RGB";
		inline constexpr const char* FresnelNode    = "Mat.Fresnel";
		inline constexpr const char* LayerWeight    = "Mat.LayerWeight";
		inline constexpr const char* TimeNode       = "Mat.Time";

		// ---- Texture ----
		inline constexpr const char* ImageTexture   = "Mat.ImageTexture";
		inline constexpr const char* NoiseTexture   = "Mat.NoiseTexture";
		inline constexpr const char* VoronoiTexture = "Mat.VoronoiTexture";
		inline constexpr const char* WaveTexture    = "Mat.WaveTexture";
		inline constexpr const char* CheckerTexture = "Mat.CheckerTexture";
		inline constexpr const char* GradientTexture= "Mat.GradientTexture";

		// ---- Color ----
		inline constexpr const char* MixNode        = "Mat.Mix";
		inline constexpr const char* InvertNode     = "Mat.Invert";
		inline constexpr const char* HueSatValNode  = "Mat.HueSatVal";
		inline constexpr const char* BrightContrast = "Mat.BrightContrast";
		inline constexpr const char* GammaNode      = "Mat.Gamma";

		// ---- Vector ----
		inline constexpr const char* MappingNode    = "Mat.Mapping";
		inline constexpr const char* NormalMapNode  = "Mat.NormalMap";
		inline constexpr const char* BumpNode       = "Mat.Bump";
		inline constexpr const char* VectorMathNode = "Mat.VectorMath";

		// ---- Converter ----
		inline constexpr const char* MathNode       = "Mat.Math";
		inline constexpr const char* MapRangeNode   = "Mat.MapRange";
		inline constexpr const char* ClampNode      = "Mat.Clamp";
		inline constexpr const char* ColorRampNode  = "Mat.ColorRamp";
		inline constexpr const char* SeparateColor  = "Mat.SeparateColor";
		inline constexpr const char* CombineColor   = "Mat.CombineColor";
		inline constexpr const char* SeparateXYZ    = "Mat.SeparateXYZ";
		inline constexpr const char* CombineXYZ     = "Mat.CombineXYZ";
		inline constexpr const char* RGBToBW        = "Mat.RGBToBW";

		// ---- Legacy (first node-graph version; still loadable, hidden from menus) ----
		inline constexpr const char* OutputNode   = "Material.Output";
		inline constexpr const char* TextureNode  = "Material.TextureSample";
		inline constexpr const char* ColorNode    = "Material.Color";
		inline constexpr const char* FloatNode    = "Material.Float";
		inline constexpr const char* MultiplyNode = "Material.Multiply";

		const Graph::NodeRegistry& GetRegistry();

		// Color ramp stops, stored as "pos:r,g,b,a;pos:r,g,b,a" in Properties["Stops"]
		struct RampStop { float Position = 0.0f; glm::vec4 Color{ 1.0f }; };
		std::vector<RampStop> ParseRamp(const std::string& text);
		std::string WriteRamp(const std::vector<RampStop>& stops);

		// Build a graph (Principled BSDF + textures) reproducing the material's slider setup
		std::shared_ptr<Graph::NodeGraph> CreateFromMaterial(const MaterialAsset& material);

		// Bake what the fixed PBR shader can express into the material parameters. Returns warnings.
		std::vector<std::string> Apply(const Graph::NodeGraph& graph, MaterialAsset& material);

	}

} // namespace Lunex
