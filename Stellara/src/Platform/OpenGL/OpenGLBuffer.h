#pragma once

#include "Renderer/Buffer.h"

namespace Stellara {

	class STELLARA_API OpenGLVertexBuffer : public VertexBuffer {
		
		public:
			OpenGLVertexBuffer(float* vertices, uint32_t size);
			virtual ~OpenGLVertexBuffer();

			virtual void Bind() const override;
			virtual void Unbind() const override;

			virtual const BufferLayout& GetLayout() const override { return m_Layout; }
			virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
			
		private:
			uint32_t m_RendererID;
			BufferLayout m_Layout;
	};

	class STELLARA_API OpenGLIndexBuffer : public IndexBuffer {
		
		public:
			OpenGLIndexBuffer(unsigned int* indices, uint32_t count);
			virtual ~OpenGLIndexBuffer();

			virtual void Bind() const override;
			virtual void Unbind() const override;
			
			virtual uint32_t GetCount() const override { return m_Count; }

		private:
			uint32_t m_RendererID;
			uint32_t m_Count;
	};
} // namespace Stellara