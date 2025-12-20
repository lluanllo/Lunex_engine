#pragma once

/**
 * @file RenderCommand.h
 * @brief Immediate-mode rendering commands facade
 * 
 * @note DEPRECATION NOTICE: This class is being phased out in favor of the RHI layer.
 * 
 * Migration Path:
 * - For new code: Use RHI::RHICommandList directly
 * - For existing code: Continue using RenderCommand until migrated
 * 
 * Usage in Legacy Code:
 * - Renderer2D::Flush() uses RenderCommand for batched 2D rendering
 * - Mesh::Draw() uses RenderCommand for 3D mesh rendering
 * - Renderer::Init() uses RenderCommand for OpenGL initialization
 * 
 * RHI Equivalents:
 * - RenderCommand::DrawIndexed() -> RHICommandList::DrawIndexed()
 * - RenderCommand::SetViewport() -> RHICommandList::SetViewport()
 * - RenderCommand::Clear() -> RHICommandList::ClearRenderTarget()
 * 
 * This class will be removed once:
 * 1. Renderer2D is migrated to use RHI command lists
 * 2. Mesh rendering uses RHI pipelines
 * 3. All immediate OpenGL state is managed through RHI
 */

#include "RendererAPI.h"

namespace Lunex {
	
	/**
	 * @class RenderCommand
	 * @brief Static facade for immediate-mode rendering commands
	 * 
	 * @note For new rendering code, prefer using RHI::RHICommandList.
	 *       This class remains for backward compatibility with Renderer2D and Mesh.
	 */
	class RenderCommand {
		public:
			static void Init() {
				s_RendererAPI->Init();
			}
			
			static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
				s_RendererAPI->SetViewport(x, y, width, height);
			}
			
			static void GetViewport(int* viewport) {
				s_RendererAPI->GetViewport(viewport);
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
			
			static void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) {
				s_RendererAPI->DrawArrays(vertexArray, vertexCount);
			}
			
			static void SetLineWidth(float width) {
				s_RendererAPI->SetLineWidth(width);
			}
			
			static void SetDepthMask(bool enabled) {
				s_RendererAPI->SetDepthMask(enabled);
			}
			
			static void SetDepthFunc(DepthFunc func) {
				s_RendererAPI->SetDepthFunc(func);
			}
			
			static DepthFunc GetDepthFunc() {
				return s_RendererAPI->GetDepthFunc();
			}
			
			static void SetDrawBuffers(const std::vector<uint32_t>& attachments) {
				s_RendererAPI->SetDrawBuffers(attachments);
			}
			
			// Viewport helper: save/restore
			static void SaveViewport(int outViewport[4]) {
				s_RendererAPI->GetViewport(outViewport);
			}
			
			static void RestoreViewport(int viewport[4]) {
				s_RendererAPI->SetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			}
			
		private:
			static Scope<RendererAPI> s_RendererAPI;
	};
}