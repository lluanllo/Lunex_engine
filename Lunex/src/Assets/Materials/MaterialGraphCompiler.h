#pragma once

/**
 * @file MaterialGraphCompiler.h
 * @brief Material node graph -> GLSL, plus the GPU runtime that renders with it.
 *
 * The generated code defines `LnxBSDF lnx_evaluate_graph()` and is injected into the
 * deferred geometry shader and the forward Mesh3D shader (LNX_MATERIAL_GRAPH variant).
 *
 *  - Constants (unlinked socket values, Value/RGB nodes, ramp stops) are read from the
 *    GraphParams UBO (binding 11): u_GraphParams[0].x = time, then one vec4 per param.
 *    Editing a value only refreshes the UBO.
 *  - Structure (nodes, links, operations) determines the code; the GL program is rebuilt
 *    only when the code hash changes.
 *  - Image textures are bound to sampler units 0-7 and 12-15 (the classic material maps
 *    and detail normals are unused by graph materials).
 */

#include "Core/Core.h"
#include "Graph/NodeGraph.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace Lunex {

	class Shader;
	class Texture2D;
	class UniformBuffer;

	namespace MaterialGraph {

		inline constexpr int kMaxGraphParams = 512;      // vec4 slots incl. slot 0 (time)
		inline constexpr int kMaxGraphTextures = 12;
		inline constexpr uint32_t kGraphParamsBinding = 11;

		// Where a parameter's value comes from
		struct ParamRef {
			Graph::ID Node = 0;
			Graph::ID Pin = 0;        // 0 = Node::Value
			int RampStop = -1;        // >= 0: color ramp stop (or curve point when Curve is set)
			bool RampPosition = false;
			std::string Curve;        // curve property key ("Curve R"...): value = (x, y, tangent, 0)
		};

		struct CompiledGraph {
			std::string Code;                                   // GLSL helpers + lnx_evaluate_graph()
			uint64_t CodeHash = 0;
			std::vector<ParamRef> Params;                       // index i -> u_GraphParams[i + 1]
			std::unordered_map<std::string, int> NamedParams;   // Value/RGB node Name -> param index
			std::vector<std::string> TexturePaths;              // texture i -> sampler slot SlotForTexture(i)
			std::vector<std::string> Errors;
			bool Forward = false;    // rendered after lighting (transparent pass / Shader to RGB), not in the G-Buffer
			bool Blended = false;    // sorted, alpha blended, with screen-space refraction
			bool IsValid() const { return Errors.empty() && !Code.empty(); }
		};

		CompiledGraph Compile(const Graph::NodeGraph& graph);
		// Every node of `graph` in one function: `vec4 lnx_preview(int target)`, target = index in `order`
		CompiledGraph CompilePreviews(const Graph::NodeGraph& graph, std::vector<Graph::ID>& order);
		uint32_t SlotForTexture(int index);
		// Current value of a parameter (socket default, Value/RGB node, ramp stop, curve point)
		glm::vec4 EvaluateParam(const Graph::NodeGraph& graph, const ParamRef& ref);

		// GPU side of a graph material: GL programs (deferred + forward), textures and params
		class GraphRuntime {
		public:
			enum class Mode { Deferred = 0, Forward = 1, ForwardHDR = 2 };
			// Re-generate code; keeps the GL programs when the code did not change
			void Update(const Graph::NodeGraph& graph);
			const CompiledGraph& GetCompiled() const { return m_Compiled; }

			// Binds program + params + textures. Returns the program to draw with, or null
			// (graph invalid / shader failed) so the caller falls back to the classic path.
			Ref<Shader> Bind(bool deferred, const Graph::NodeGraph& graph,
				const std::unordered_map<std::string, glm::vec4>* overrides = nullptr);
			Ref<Shader> Bind(Mode mode, const Graph::NodeGraph& graph,
				const std::unordered_map<std::string, glm::vec4>* overrides = nullptr);
			// Backend-neutral data for one draw: params (slot 0 = time, then one vec4 per param,
			// instance overrides applied) and the image textures (null = missing). Used by the
			// Vulkan renderer, which builds its own pipelines from GetCompiled().Code.
			void GatherData(const Graph::NodeGraph& graph, const std::unordered_map<std::string, glm::vec4>* overrides,
				std::vector<glm::vec4>& outParams, std::vector<Ref<Texture2D>>& outTextures);
			bool IsForward() const { return m_Compiled.IsValid() && m_Compiled.Forward; }
			bool IsBlended() const { return m_Compiled.IsValid() && m_Compiled.Blended; }

			bool HasShaderError() const { return m_ShaderFailed; }

		private:
			Ref<Shader> GetOrCreateShader(Mode mode);
			void RefreshParams(const Graph::NodeGraph& graph);
			void RefreshTextures();

			CompiledGraph m_Compiled;
			Ref<Shader> m_Shaders[3];
			bool m_Tried[3] = { false, false, false };
			bool m_ShaderFailed = false;
			std::vector<Ref<Texture2D>> m_Textures;
			std::vector<glm::vec4> m_Params;
			uint64_t m_ParamsVersion = 0;
		};

		// Source of the shader templates (relative to the editor working directory)
		std::string BuildShaderSource(const std::string& templatePath, const std::string& graphCode, const char* stageDefine, const char* extraDefine = nullptr);

	}

} // namespace Lunex
