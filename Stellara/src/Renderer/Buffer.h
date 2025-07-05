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

	struct BufferElement {
		std::string Name;
		ShaderDataType Type;
		uint32_t Size;
		uint32_t Offset;

		BufferElement(ShaderDataType type, const std::string& name)
			: Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0) {

		}
	};

	class STELLARA_API BufferLayout {
		public:
			BufferLayout(const std::initializer_list<BufferElement> element)
				: m_Elements(element) {
				CalculateOffsetAndStride();
			}
			inline const std::vector<BufferElement>& GetElements() const { return m_Elements; }
	
		private:
			void CalculateOffsetAndStride(){
				
				m_Stride = 0;
				m_Offset = 0;
				
				for (auto& element : m_Elements) {
					element.Offset = m_Offset;
					m_Offset += element.Size;
					m_Stride += element.Size;
				}
				
			}

		private:
			std::vector<BufferElement> m_Elements;
			uint32_t m_Stride;
			uint32_t m_Offset;
	};
	
	class STELLARA_API VertexBuffer {
		public:
			virtual ~VertexBuffer() {}
			
			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;
			
			virtual void GetLayout() const = 0;
			virtual void SetLayout(const BufferLayout& layout) = 0;
			
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