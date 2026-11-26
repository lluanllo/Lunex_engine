#include "stpch.h"
#include "MaterialGraphPreview.h"
#include "MaterialGraph.h"

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/FrameBuffer.h"
#include "RHI/RHI.h"
#include "Log/Log.h"

#include <glad/glad.h>

#include <cstdlib>
#include <fstream>

namespace Lunex::MaterialGraph {

	using namespace Graph;

	namespace {
		// Flat UV plane: Generated = (u, v, 0), Normal = +Z (like Blender's flat previews)
		const char* kPreviewTemplate = R"GLSL(
#ifdef VERTEX
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;
layout(location = 0) out vec2 v_UV;
void main() {
	v_UV = a_TexCoords;
	gl_Position = vec4(a_Position, 1.0);
}
#elif defined(FRAGMENT)
layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

#define LNX_UV v_UV
#define LNX_POS vec3(v_UV, 0.0)
#define LNX_NORMAL vec3(0.0, 0.0, 1.0)
#define LNX_TBN mat3(1.0)
#define LNX_VIEWPOS vec3(0.5, 0.5, 2.0)
#define LNX_VIEW mat4(1.0)
#define LNX_VIEWPROJ mat4(1.0)
#define LNX_MODEL mat4(1.0)
#define LNX_MODEL_INV mat4(1.0)
#define LNX_OBJECT_ID 0.0
#define LNX_OBJPOS LNX_POS

vec3 lnx_light(vec3 albedo, float metallic, float roughness, float specular, vec3 N, float ao) {
	vec3 L = normalize(vec3(0.4, 0.5, 0.8));
	return albedo * (1.0 - metallic) * (0.25 + 0.75 * max(dot(N, L), 0.0)) * ao;
}

layout(std140, binding = 11) uniform GraphParams {
	vec4 u_GraphParams[512];   // [0].x = time, [0].y = node to preview
};
layout (binding = 0) uniform sampler2D u_GraphTex0;
layout (binding = 1) uniform sampler2D u_GraphTex1;
layout (binding = 2) uniform sampler2D u_GraphTex2;
layout (binding = 3) uniform sampler2D u_GraphTex3;
layout (binding = 4) uniform sampler2D u_GraphTex4;
layout (binding = 5) uniform sampler2D u_GraphTex5;
layout (binding = 6) uniform sampler2D u_GraphTex6;
layout (binding = 7) uniform sampler2D u_GraphTex7;
layout (binding = 12) uniform sampler2D u_GraphTex8;
layout (binding = 13) uniform sampler2D u_GraphTex9;
layout (binding = 14) uniform sampler2D u_GraphTex10;
layout (binding = 15) uniform sampler2D u_GraphTex11;

// @LNX_GRAPH_CODE@

void main() {
	vec4 c = lnx_preview(int(u_GraphParams[0].y + 0.5));
	// Display transform (linear -> sRGB), like Blender's previews
	vec3 rgb = pow(clamp(c.rgb, 0.0, 1.0), vec3(1.0 / 2.2));
	o_Color = vec4(rgb, 1.0);
}
#endif
)GLSL";

		std::string MakeStage(const char* define, const std::string& code) {
			std::string src = kPreviewTemplate;
			const std::string marker = "// @LNX_GRAPH_CODE@";
			src.replace(src.find(marker), marker.size(), code);
			return std::string("#version 450 core\n#define ") + define + "\n#define LNX_MATERIAL_GRAPH\n" + src;
		}
	}

	NodePreviewRenderer::NodePreviewRenderer(uint32_t tileSize) : m_TileSize(tileSize) {}

	bool NodePreviewRenderer::GetTile(ID node, uint32_t& textureID, glm::vec2& uv0, glm::vec2& uv1) const {
		auto it = m_Index.find(node);
		if (it == m_Index.end() || !m_Atlas || m_Rows == 0) return false;
		const int i = it->second;
		const float col = static_cast<float>(i % m_Cols), row = static_cast<float>(i / m_Cols);
		const float W = static_cast<float>(m_Cols), H = static_cast<float>(m_Rows);
		textureID = m_Atlas->GetColorAttachmentRendererID(0);
		// GL textures are bottom-up: the top of the tile is the larger v
		uv0 = { col / W, (row + 1.0f) / H };
		uv1 = { (col + 1.0f) / W, row / H };
		return true;
	}

	void NodePreviewRenderer::Update(const NodeGraph& graph) {
		if (&graph == m_Graph && graph.GetVersion() == m_Version) return;
		m_Graph = &graph;
		m_Version = graph.GetVersion();

		std::vector<ID> order;
		CompiledGraph compiled = CompilePreviews(graph, order);
		// Previews tolerate graph errors (unsupported nodes output zero); only a missing body stops them
		if (compiled.Code.empty() || order.empty()) { m_Index.clear(); return; }
		if (!compiled.Errors.empty() && m_LastError != compiled.Errors.front()) {
			m_LastError = compiled.Errors.front();
			LNX_LOG_WARN("Node previews: {0}", m_LastError);
		}

		// Shader: only when the structure changed
		if (compiled.CodeHash != m_CodeHash) {
			if (std::getenv("LUNEX_DUMP_GRAPH_CODE")) std::ofstream("Cache/PreviewGraphCode.glsl") << compiled.Code;
			m_CodeHash = compiled.CodeHash;
			char name[64];
			snprintf(name, sizeof(name), "NodePreview_%016llx", static_cast<unsigned long long>(compiled.CodeHash));
			m_Shader = Shader::Create(name, MakeStage("VERTEX", compiled.Code), MakeStage("FRAGMENT", compiled.Code));
			m_ShaderFailed = !m_Shader || !m_Shader->IsValid();
			if (m_ShaderFailed) {
				LNX_LOG_WARN("Node previews: shader failed to compile (source saved to Cache/FailedPreviewShader.glsl)");
				std::ofstream("Cache/FailedPreviewShader.glsl") << MakeStage("FRAGMENT", compiled.Code);
				m_Shader.reset();
			}
		}
		if (compiled.TexturePaths != m_Compiled.TexturePaths) m_Textures.clear();
		m_Compiled = std::move(compiled);
		if (m_ShaderFailed) { m_Index.clear(); return; }

		m_Index.clear();
		for (size_t i = 0; i < order.size(); i++) m_Index[order[i]] = static_cast<int>(i);

		// Params, same layout as GraphRuntime (slot 0 = time / preview target)
		m_Params.assign(kMaxGraphParams, glm::vec4(0.0f));
		for (size_t i = 0; i < m_Compiled.Params.size() && i + 1 < kMaxGraphParams; i++)
			m_Params[i + 1] = EvaluateParam(graph, m_Compiled.Params[i]);

		Render();
	}

	void NodePreviewRenderer::Render() {
		if (!m_Shader) return;
		const uint32_t count = static_cast<uint32_t>(m_Index.size());
		m_Rows = (count + m_Cols - 1) / m_Cols;
		const uint32_t W = m_Cols * m_TileSize, H = std::max(1u, m_Rows) * m_TileSize;

		if (!m_Atlas) {
			FramebufferSpecification spec;
			spec.Width = W;
			spec.Height = H;
			spec.Attachments = { FramebufferTextureFormat::RGBA8 };
			m_Atlas = Framebuffer::Create(spec);
		}
		else if (m_Atlas->GetSpecification().Width != W || m_Atlas->GetSpecification().Height != H) {
			m_Atlas->Resize(W, H);
		}
		if (!m_Quad) {
			static float quad[] = {
				-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,   1.0f, -1.0f, 0.0f, 1.0f, 0.0f,   -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
				 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,   1.0f,  1.0f, 0.0f, 1.0f, 1.0f,   -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
			};
			m_Quad = VertexArray::Create();
			m_QuadVB = VertexBuffer::Create(quad, sizeof(quad));
			m_QuadVB->SetLayout({ { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float2, "a_TexCoords" } });
			m_Quad->AddVertexBuffer(m_QuadVB);
		}
		if (!m_ParamsUBO) m_ParamsUBO = UniformBuffer::Create(sizeof(glm::vec4) * kMaxGraphParams, kGraphParamsBinding);

		// Textures (shared cache with the material runtime would need its API; keep a local one)
		if (m_Textures.size() != m_Compiled.TexturePaths.size()) {
			static auto& s_Cache = *new std::unordered_map<std::string, Ref<Texture2D>>();   // leaked on purpose (GL)
			m_Textures.clear();
			for (const auto& path : m_Compiled.TexturePaths) {
				auto& cached = s_Cache[path];
				if (!cached || !cached->IsLoaded()) cached = Texture2D::Create(path);
				m_Textures.push_back(cached && cached->IsLoaded() ? cached : nullptr);
			}
		}

		// Save the GL state we touch
		GLint prevFB = 0, prevViewport[4];
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFB);
		glGetIntegerv(GL_VIEWPORT, prevViewport);
		const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
		glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);

		m_Atlas->Bind();
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		m_Shader->Bind();
		for (size_t i = 0; i < m_Textures.size(); i++)
			if (m_Textures[i]) m_Textures[i]->Bind(SlotForTexture(static_cast<int>(i)));
		m_Quad->Bind();
		auto* cmd = RHI::GetImmediateCommandList();
		for (uint32_t i = 0; i < count; i++) {
			m_Params[0].y = static_cast<float>(i);
			m_ParamsUBO->SetData(m_Params.data(), sizeof(glm::vec4) * kMaxGraphParams);
			glViewport(static_cast<GLint>((i % m_Cols) * m_TileSize), static_cast<GLint>((i / m_Cols) * m_TileSize), m_TileSize, m_TileSize);
			if (cmd) cmd->DrawArrays(6);
		}
		m_Atlas->Unbind();

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevFB));
		glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
		if (blend) glEnable(GL_BLEND);
		if (depth) glEnable(GL_DEPTH_TEST);
		if (cull) glEnable(GL_CULL_FACE);
		if (scissor) glEnable(GL_SCISSOR_TEST);
	}

}
