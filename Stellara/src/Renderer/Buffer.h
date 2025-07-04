#pragma once

namespace Stellara {

	enum class ShaderDataType {
		None = 0,
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4,
		Mat3, Mat4,
		Bool
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType type) {
		switch (type) {
			case ShaderDataType::None:   return 0;
			case ShaderDataType::Float:  return 4;
			case ShaderDataType::Float2: return 4 * 2;
			case ShaderDataType::Float3: return 4 * 3;
			case ShaderDataType::Float4: return 4 * 4;
			case ShaderDataType::Int:    return 4;
			case ShaderDataType::Int2:   return 4 * 2;
			case ShaderDataType::Int3:   return 4 * 3;
			case ShaderDataType::Int4:   return 4 * 4;
			case ShaderDataType::Mat3:   return 4 * 3 * 3;
			case ShaderDataType::Mat4:   return 4 * 4 * 4;
			case ShaderDataType::Bool:   return 1;
		}
		ST_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0; // Should never reach here
	}

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