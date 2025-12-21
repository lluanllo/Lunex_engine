#include "stpch.h"
#include "Buffer.h"

#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHIBuffer.h"

namespace Lunex {
	
	// ============================================================================
	// UTILITY FUNCTIONS - Convert legacy BufferLayout to RHI::VertexLayout
	// ============================================================================
	
	namespace {
		RHI::DataType ConvertToRHIDataType(ShaderDataType type) {
			switch (type) {
				case ShaderDataType::Float:  return RHI::DataType::Float;
				case ShaderDataType::Float2: return RHI::DataType::Float2;
				case ShaderDataType::Float3: return RHI::DataType::Float3;
				case ShaderDataType::Float4: return RHI::DataType::Float4;
				case ShaderDataType::Int:    return RHI::DataType::Int;
				case ShaderDataType::Int2:   return RHI::DataType::Int2;
				case ShaderDataType::Int3:   return RHI::DataType::Int3;
				case ShaderDataType::Int4:   return RHI::DataType::Int4;
				case ShaderDataType::Mat3:   return RHI::DataType::Mat3;
				case ShaderDataType::Mat4:   return RHI::DataType::Mat4;
				case ShaderDataType::Bool:   return RHI::DataType::Bool;
				default:                     return RHI::DataType::Float;
			}
		}
		
		RHI::VertexLayout ConvertToRHIVertexLayout(const BufferLayout& layout) {
			RHI::VertexLayout rhiLayout;
			for (const auto& element : layout.GetElements()) {
				rhiLayout.AddAttribute(RHI::VertexAttribute(element.Name, ConvertToRHIDataType(element.Type), element.Normalized));
			}
			return rhiLayout;
		}
	}
	
	// ============================================================================
	// VERTEX BUFFER ADAPTER - Wraps RHIVertexBuffer with legacy interface
	// ============================================================================
	
	class VertexBufferAdapter : public VertexBuffer {
	public:
		VertexBufferAdapter(uint32_t size)
		{
			RHI::BufferDesc desc;
			desc.Type = RHI::BufferType::Vertex;
			desc.Size = size;
			desc.Usage = RHI::BufferUsage::Dynamic;
			
			m_RHIBuffer = CreateRef<RHI::OpenGLRHIVertexBuffer>(desc, RHI::VertexLayout{}, nullptr);
		}
		
		VertexBufferAdapter(float* vertices, uint32_t size)
		{
			RHI::BufferDesc desc;
			desc.Type = RHI::BufferType::Vertex;
			desc.Size = size;
			desc.Usage = RHI::BufferUsage::Static;
			
			m_RHIBuffer = CreateRef<RHI::OpenGLRHIVertexBuffer>(desc, RHI::VertexLayout{}, vertices);
		}
		
		virtual ~VertexBufferAdapter() = default;
		
		void Bind() const override {
			m_RHIBuffer->Bind();
		}
		
		void Unbind() const override {
			m_RHIBuffer->Unbind();
		}
		
		void SetData(const void* data, uint32_t size) override {
			m_RHIBuffer->SetData(data, size);
		}
		
		const BufferLayout& GetLayout() const override {
			return m_Layout;
		}
		
		void SetLayout(const BufferLayout& layout) override {
			m_Layout = layout;
			// Convert and set RHI layout
			auto rhiLayout = ConvertToRHIVertexLayout(layout);
			m_RHIBuffer->SetLayout(rhiLayout);
		}
		
		Ref<RHI::RHIVertexBuffer> GetRHIBuffer() const { return m_RHIBuffer; }
		
	private:
		Ref<RHI::OpenGLRHIVertexBuffer> m_RHIBuffer;
		BufferLayout m_Layout;
	};
	
	// ============================================================================
	// INDEX BUFFER ADAPTER - Wraps RHIIndexBuffer with legacy interface
	// ============================================================================
	
	class IndexBufferAdapter : public IndexBuffer {
	public:
		IndexBufferAdapter(unsigned int* indices, uint32_t count)
			: m_Count(count)
		{
			RHI::BufferDesc desc;
			desc.Type = RHI::BufferType::Index;
			desc.Size = count * sizeof(uint32_t);
			desc.Usage = RHI::BufferUsage::Static;
			desc.IndexFormat = RHI::IndexType::UInt32;
			
			m_RHIBuffer = CreateRef<RHI::OpenGLRHIIndexBuffer>(desc, indices);
		}
		
		virtual ~IndexBufferAdapter() = default;
		
		void Bind() const override {
			m_RHIBuffer->Bind();
		}
		
		void Unbind() const override {
			m_RHIBuffer->Unbind();
		}
		
		uint32_t GetCount() const override {
			return m_Count;
		}
		
		Ref<RHI::RHIIndexBuffer> GetRHIBuffer() const { return m_RHIBuffer; }
		
	private:
		Ref<RHI::OpenGLRHIIndexBuffer> m_RHIBuffer;
		uint32_t m_Count;
	};
	
	// ============================================================================
	// FACTORY IMPLEMENTATIONS
	// ============================================================================
	
	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:    
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!"); 
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:  
				return CreateRef<VertexBufferAdapter>(size);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
	
	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:    
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!"); 
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:  
				return CreateRef<VertexBufferAdapter>(vertices, size);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
	
	Ref<IndexBuffer> IndexBuffer::Create(unsigned int* indices, uint32_t count) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:    
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!"); 
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:  
				return CreateRef<IndexBufferAdapter>(indices, count);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
}