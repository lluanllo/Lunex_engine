#pragma once

#include "RendererAPI.h"

namespace Lunex {
	class RenderCommand {
		public:
			static void Init() {
				s_RendererAPI->Init();
			}
			
			static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
				s_RendererAPI->SetViewport(x, y, width, height);
			}
			
			static void SetClearColor(const glm::vec4& color) {
				s_RendererAPI->SetClearColor(color);
			}
			
			static void Clear() {
				s_RendererAPI->Clear();
			}
			
			static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) {
				s_RendererAPI->DrawIndexed(vertexArray, indexCount);
			}
			
			static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) {
				s_RendererAPI->DrawLines(vertexArray, vertexCount);
			}
			
			static void SetLineWidth(float width) {
				s_RendererAPI->SetLineWidth(width);
			}

			static void SetDepthTest(bool enabled) {
				s_RendererAPI->SetDepthTest(enabled);
			}

			static void SetDepthFunc(RendererAPI::DepthFunc func) {
				s_RendererAPI->SetDepthFunc(func);
			}

			static void SetDepthMask(bool enabled) {
				s_RendererAPI->SetDepthMask(enabled);
			}

			static void SetCullMode(RendererAPI::CullMode mode) {
				s_RendererAPI->SetCullMode(mode);
			}

			static void SetBlend(bool enabled) {
				s_RendererAPI->SetBlend(enabled);
			}
			
			static void SetBlendFunc(RendererAPI::BlendFactor src, RendererAPI::BlendFactor dst) {
				s_RendererAPI->SetBlendFunc(src, dst);
			}
		private:
			static Scope<RendererAPI> s_RendererAPI;
	};
}