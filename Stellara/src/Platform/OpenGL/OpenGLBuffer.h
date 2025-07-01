#pragma once

#include "Renderer/Buffer.h"

namespace Stellara {

	class OpenGLVertexBuffer : public VertexBuffer {
		
		public:
			OpenGLVertexBuffer(float* vertices, uint32_t size);
			virtual ~OpenGLVertexBuffer();

			virtual void Bind() const override;
			virtual void Unbind() const override;
			
		private:
			uint32_t m_RendererID;
	};

	class OpenGLIndexBuffer : public IndexBuffer {
		
		public:
			OpenGLIndexBuffer(unsigned int* indices, uint32_t size);
			virtual ~OpenGLIndexBuffer();

			virtual void Bind() const override;
			virtual void Unbind() const override;
			
		private:
			uint32_t m_RendererID;
	};
} // namespace Stellara