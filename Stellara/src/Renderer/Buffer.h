#pragma once

namespace Stellara {
	
	class STELLARA_API VertexBuffer {
		public:
			virtual ~VertexBuffer() {}
			
			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;
			
			static VertexBuffer* Create(float* vertices, uint32_t size);
	};
	
	class STELLARA_API IndexBuffer {
		public:
			virtual ~IndexBuffer() {}
			
			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;
			
			virtual uint32_t GetCount() const = 0;

			static IndexBuffer* Create(unsigned int* indices, uint32_t size);
	};
}