#pragma once

#include "Renderer/RendererAPI.h"

namespace Lunex {
	class OpenGLRendererAPI : public RendererAPI {
		public:
			virtual void Init() override;
			virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
			virtual void GetViewport(int* viewport) override;
			
			virtual void SetClearColor(const glm::vec4& color) override;
			virtual void Clear() override;
			
			virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
			virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;
			virtual void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;
			
			virtual void SetLineWidth(float width) override;
			virtual void SetDepthMask(bool enabled) override;
			virtual void SetDepthFunc(DepthFunc func) override;
			virtual DepthFunc GetDepthFunc() override;
			
			virtual void SetDrawBuffers(const std::vector<uint32_t>& attachments) override;
			
		private:
			DepthFunc m_CurrentDepthFunc = DepthFunc::Less;
	};
}