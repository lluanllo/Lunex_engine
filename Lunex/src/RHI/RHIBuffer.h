#pragma once

/**
 * @file RHIBuffer.h
 * @brief GPU buffer interfaces for vertex, index, uniform, and storage buffers
 * 
 * Provides abstract interfaces for all buffer types with support for:
 * - Static and dynamic buffers
 * - Memory mapping
 * - Structured data access
 */

#include "RHIResource.h"
#include "RHITypes.h"
#include <span>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// BUFFER DESCRIPTION
	// ============================================================================
	
	struct BufferDesc {
		BufferType Type = BufferType::Vertex;
		BufferUsage Usage = BufferUsage::Static;
		uint64_t Size = 0;
		uint32_t Stride = 0;              // Element stride (for structured buffers)
		IndexType IndexFormat = IndexType::UInt32;  // For index buffers
		
		BufferDesc() = default;
		BufferDesc(BufferType type, uint64_t size, BufferUsage usage = BufferUsage::Static)
			: Type(type), Usage(usage), Size(size) {}
	};

	// ============================================================================
	// MAPPED BUFFER RANGE
	// ============================================================================
	
	/**
	 * @struct MappedBufferRange
	 * @brief Represents a mapped region of GPU memory
	 * 
	 * RAII wrapper that automatically unmaps when destroyed
	 */
	struct MappedBufferRange {
		void* Data = nullptr;
		uint64_t Offset = 0;
		uint64_t Size = 0;
		bool Valid = false;
		
		operator bool() const { return Valid && Data != nullptr; }
		
		template<typename T>
		T* As() { return reinterpret_cast<T*>(Data); }
		
		template<typename T>
		const T* As() const { return reinterpret_cast<const T*>(Data); }
		
		template<typename T>
		std::span<T> AsSpan() {
			return std::span<T>(reinterpret_cast<T*>(Data), Size / sizeof(T));
		}
	};

	// ============================================================================
	// RHI BUFFER BASE CLASS
	// ============================================================================
	
	/**
	 * @class RHIBuffer
	 * @brief Base class for all GPU buffer types
	 */
	class RHIBuffer : public RHIResource {
	public:
		virtual ~RHIBuffer() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Buffer; }
		
		// ============================================
		// BUFFER PROPERTIES
		// ============================================
		
		/**
		 * @brief Get the buffer description
		 */
		const BufferDesc& GetDesc() const { return m_Desc; }
		
		/**
		 * @brief Get the buffer size in bytes
		 */
		uint64_t GetSize() const { return m_Desc.Size; }
		
		/**
		 * @brief Get the buffer type
		 */
		BufferType GetType() const { return m_Desc.Type; }
		
		/**
		 * @brief Get the buffer usage mode
		 */
		BufferUsage GetUsage() const { return m_Desc.Usage; }
		
		/**
		 * @brief Get the element stride (for structured buffers)
		 */
		uint32_t GetStride() const { return m_Desc.Stride; }
		
		/**
		 * @brief Get element count (for structured buffers)
		 */
		uint64_t GetElementCount() const {
			return m_Desc.Stride > 0 ? m_Desc.Size / m_Desc.Stride : 0;
		}
		
		// ============================================
		// DATA OPERATIONS
		// ============================================
		
		/**
		 * @brief Upload data to the buffer
		 * @param data Pointer to source data
		 * @param size Size of data in bytes
		 * @param offset Offset in buffer to write to
		 */
		virtual void SetData(const void* data, uint64_t size, uint64_t offset = 0) = 0;
		
		/**
		 * @brief Read data from the buffer (for staging buffers)
		 * @param data Destination buffer
		 * @param size Size to read
		 * @param offset Offset to read from
		 */
		virtual void GetData(void* data, uint64_t size, uint64_t offset = 0) const = 0;
		
		/**
		 * @brief Upload typed data
		 */
		template<typename T>
		void SetData(const T* data, uint64_t count, uint64_t elementOffset = 0) {
			SetData(data, count * sizeof(T), elementOffset * sizeof(T));
		}
		
		/**
		 * @brief Upload a single value
		 */
		template<typename T>
		void SetValue(const T& value, uint64_t offset = 0) {
			SetData(&value, sizeof(T), offset);
		}
		
		// ============================================
		// MEMORY MAPPING
		// ============================================
		
		/**
		 * @brief Map the buffer for CPU access
		 * @param access Read/write access mode
		 * @return Mapped buffer range
		 */
		virtual MappedBufferRange Map(BufferAccess access = BufferAccess::Write) = 0;
		
		/**
		 * @brief Map a range of the buffer
		 * @param offset Offset to start mapping
		 * @param size Size to map (0 = rest of buffer)
		 * @param access Access mode
		 */
		virtual MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) = 0;
		
		/**
		 * @brief Unmap the buffer
		 */
		virtual void Unmap() = 0;
		
		/**
		 * @brief Flush mapped range (for non-coherent mappings)
		 * @param offset Start of range to flush
		 * @param size Size to flush
		 */
		virtual void FlushMappedRange(uint64_t offset, uint64_t size) = 0;
		
		/**
		 * @brief Check if buffer is currently mapped
		 */
		virtual bool IsMapped() const = 0;
		
		// ============================================
		// BINDING
		// ============================================
		
		/**
		 * @brief Bind the buffer (OpenGL-style, deprecated for modern APIs)
		 * Use command lists for binding in new code
		 */
		virtual void Bind() const = 0;
		
		/**
		 * @brief Unbind the buffer
		 */
		virtual void Unbind() const = 0;
		
		/**
		 * @brief Bind to a specific binding point (for uniform/storage buffers)
		 * @param bindingPoint Shader binding point
		 */
		virtual void BindToPoint(uint32_t bindingPoint) const = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override { return m_Desc.Size; }
		
	protected:
		BufferDesc m_Desc;
	};

	// ============================================================================
	// VERTEX BUFFER LAYOUT
	// ============================================================================
	
	/**
	 * @struct VertexAttribute
	 * @brief Describes a single vertex attribute
	 */
	struct VertexAttribute {
		std::string Name;
		DataType Type = DataType::Float3;
		uint32_t Offset = 0;
		bool Normalized = false;
		
		VertexAttribute() = default;
		VertexAttribute(const std::string& name, DataType type, bool normalized = false)
			: Name(name), Type(type), Normalized(normalized), Offset(0) {}
		
		uint32_t GetSize() const { return GetDataTypeSize(Type); }
		uint32_t GetComponentCount() const { return GetDataTypeComponentCount(Type); }
	};

	/**
	 * @class VertexLayout
	 * @brief Describes the layout of vertex data in a buffer
	 */
	class VertexLayout {
	public:
		VertexLayout() = default;
		
		VertexLayout(std::initializer_list<VertexAttribute> attributes)
			: m_Attributes(attributes) {
			CalculateOffsetsAndStride();
		}
		
		void AddAttribute(const VertexAttribute& attr) {
			m_Attributes.push_back(attr);
			CalculateOffsetsAndStride();
		}
		
		const std::vector<VertexAttribute>& GetAttributes() const { return m_Attributes; }
		uint32_t GetStride() const { return m_Stride; }
		
		auto begin() { return m_Attributes.begin(); }
		auto end() { return m_Attributes.end(); }
		auto begin() const { return m_Attributes.begin(); }
		auto end() const { return m_Attributes.end(); }
		
	private:
		void CalculateOffsetsAndStride() {
			uint32_t offset = 0;
			for (auto& attr : m_Attributes) {
				attr.Offset = offset;
				offset += attr.GetSize();
			}
			m_Stride = offset;
		}
		
		std::vector<VertexAttribute> m_Attributes;
		uint32_t m_Stride = 0;
	};

	// ============================================================================
	// SPECIALIZED BUFFER INTERFACES
	// ============================================================================
	
	/**
	 * @class RHIVertexBuffer
	 * @brief Specialized interface for vertex buffers with layout information
	 */
	class RHIVertexBuffer : public RHIBuffer {
	public:
		virtual ~RHIVertexBuffer() = default;
		
		/**
		 * @brief Set the vertex layout for this buffer
		 */
		virtual void SetLayout(const VertexLayout& layout) = 0;
		
		/**
		 * @brief Get the vertex layout
		 */
		virtual const VertexLayout& GetLayout() const = 0;
		
		/**
		 * @brief Get the number of vertices
		 */
		uint64_t GetVertexCount() const {
			const auto& layout = GetLayout();
			return layout.GetStride() > 0 ? m_Desc.Size / layout.GetStride() : 0;
		}
	};

	/**
	 * @class RHIIndexBuffer
	 * @brief Specialized interface for index buffers
	 */
	class RHIIndexBuffer : public RHIBuffer {
	public:
		virtual ~RHIIndexBuffer() = default;
		
		/**
		 * @brief Get the index format
		 */
		IndexType GetIndexFormat() const { return m_Desc.IndexFormat; }
		
		/**
		 * @brief Get the number of indices
		 */
		uint32_t GetIndexCount() const {
			return static_cast<uint32_t>(m_Desc.Size / GetIndexTypeSize(m_Desc.IndexFormat));
		}
	};

	/**
	 * @class RHIUniformBuffer
	 * @brief Specialized interface for uniform/constant buffers
	 */
	class RHIUniformBuffer : public RHIBuffer {
	public:
		virtual ~RHIUniformBuffer() = default;
		
		/**
		 * @brief Bind to shader binding point
		 * @param bindingPoint UBO binding index
		 */
		virtual void Bind(uint32_t bindingPoint) const = 0;
	};

	/**
	 * @class RHIStorageBuffer
	 * @brief Specialized interface for storage/structured buffers (SSBO)
	 */
	class RHIStorageBuffer : public RHIBuffer {
	public:
		virtual ~RHIStorageBuffer() = default;
		
		/**
		 * @brief Bind for compute shader read/write
		 * @param bindingPoint SSBO binding index
		 */
		virtual void BindForCompute(uint32_t bindingPoint) const = 0;
		
		/**
		 * @brief Bind for shader read-only access
		 */
		virtual void BindForRead(uint32_t bindingPoint) const = 0;
	};

} // namespace RHI
} // namespace Lunex
