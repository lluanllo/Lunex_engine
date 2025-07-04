#pragma once

namespace Stellara {

	enum class ShaderDataType {
		None = 0,
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4,
		Mat3, Mat4,
		Bool
	};

	struct BufferElements {
		std::string Name;
		ShaderDataType Type;
		uint32_t Size;
		uint32_t Offset;

		BufferElements(const std::string& name, ShaderDataType type)
			: Name(name), Type(type), Size(0), Offset(0) {

		}
	};

	class STELLARA_API BufferLayout {
		public:
				inline const std::vector<BufferElements>& GetElements() const { return m_Elements; }
		private:
			std::vector<BufferElements> m_Elements;
	};
	
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