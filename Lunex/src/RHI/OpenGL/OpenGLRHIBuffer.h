#pragma once

/**
 * @file OpenGLRHIBuffer.h
 * @brief OpenGL implementation of RHI buffer types
 */

#include "RHI/RHIBuffer.h"
#include <glad/glad.h>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL BUFFER UTILITIES
	// ============================================================================
	
	namespace OpenGLBufferUtils {
		
		inline GLenum GetBufferTarget(BufferType type) {
			switch (type) {
				case BufferType::Vertex:   return GL_ARRAY_BUFFER;
				case BufferType::Index:    return GL_ELEMENT_ARRAY_BUFFER;
				case BufferType::Uniform:  return GL_UNIFORM_BUFFER;
				case BufferType::Storage:  return GL_SHADER_STORAGE_BUFFER;
				case BufferType::Indirect: return GL_DRAW_INDIRECT_BUFFER;
				case BufferType::Staging:  return GL_COPY_WRITE_BUFFER;
				default: return GL_ARRAY_BUFFER;
			}
		}
		
		inline GLenum GetBufferUsage(BufferUsage usage) {
			switch (usage) {
				case BufferUsage::Static:  return GL_STATIC_DRAW;
				case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
				case BufferUsage::Stream:  return GL_STREAM_DRAW;
				case BufferUsage::Staging: return GL_DYNAMIC_READ;
				default: return GL_STATIC_DRAW;
			}
		}
		
		inline GLbitfield GetMapAccess(BufferAccess access) {
			GLbitfield flags = 0;
			if (static_cast<uint8_t>(access) & static_cast<uint8_t>(BufferAccess::Read))
				flags |= GL_MAP_READ_BIT;
			if (static_cast<uint8_t>(access) & static_cast<uint8_t>(BufferAccess::Write))
				flags |= GL_MAP_WRITE_BIT;
			if (static_cast<uint8_t>(access) & static_cast<uint8_t>(BufferAccess::Persistent))
				flags |= GL_MAP_PERSISTENT_BIT;
			if (static_cast<uint8_t>(access) & static_cast<uint8_t>(BufferAccess::Coherent))
				flags |= GL_MAP_COHERENT_BIT;
			return flags;
		}
		
	} // namespace OpenGLBufferUtils

	// ============================================================================
	// OPENGL RHI BUFFER
	// ============================================================================
	
	class OpenGLRHIBuffer : public RHIBuffer {
	public:
		OpenGLRHIBuffer(const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~OpenGLRHIBuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_BufferID); }
		bool IsValid() const override { return m_BufferID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;
		
		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;
		
		// OpenGL specific
		GLuint GetBufferID() const { return m_BufferID; }
		GLenum GetTarget() const { return m_Target; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_BufferID = 0;
		GLenum m_Target = GL_ARRAY_BUFFER;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// OPENGL RHI VERTEX BUFFER
	// ============================================================================
	
	class OpenGLRHIVertexBuffer : public RHIVertexBuffer {
	public:
		OpenGLRHIVertexBuffer(const BufferDesc& desc, const VertexLayout& layout, const void* initialData = nullptr);
		virtual ~OpenGLRHIVertexBuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_BufferID); }
		bool IsValid() const override { return m_BufferID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;
		
		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;
		
		// Vertex layout
		void SetLayout(const VertexLayout& layout) override { m_Layout = layout; }
		const VertexLayout& GetLayout() const override { return m_Layout; }
		
		// OpenGL specific
		GLuint GetBufferID() const { return m_BufferID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_BufferID = 0;
		VertexLayout m_Layout;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// OPENGL RHI INDEX BUFFER
	// ============================================================================
	
	class OpenGLRHIIndexBuffer : public RHIIndexBuffer {
	public:
		OpenGLRHIIndexBuffer(const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~OpenGLRHIIndexBuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_BufferID); }
		bool IsValid() const override { return m_BufferID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;
		
		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;
		
		// OpenGL specific
		GLuint GetBufferID() const { return m_BufferID; }
		GLenum GetGLIndexType() const { 
			return m_Desc.IndexFormat == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT; 
		}
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_BufferID = 0;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// OPENGL RHI UNIFORM BUFFER
	// ============================================================================
	
	class OpenGLRHIUniformBuffer : public RHIUniformBuffer {
	public:
		OpenGLRHIUniformBuffer(const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~OpenGLRHIUniformBuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_BufferID); }
		bool IsValid() const override { return m_BufferID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;
		
		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;
		
		// Uniform buffer specific
		void Bind(uint32_t bindingPoint) const override;
		
		// OpenGL specific
		GLuint GetBufferID() const { return m_BufferID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_BufferID = 0;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// OPENGL RHI STORAGE BUFFER (SSBO)
	// ============================================================================
	
	class OpenGLRHIStorageBuffer : public RHIStorageBuffer {
	public:
		OpenGLRHIStorageBuffer(const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~OpenGLRHIStorageBuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_BufferID); }
		bool IsValid() const override { return m_BufferID != 0; }
		
		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;
		
		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;
		
		// Storage buffer specific
		void BindForCompute(uint32_t bindingPoint) const override;
		void BindForRead(uint32_t bindingPoint) const override;
		
		// OpenGL specific
		GLuint GetBufferID() const { return m_BufferID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		GLuint m_BufferID = 0;
		void* m_MappedData = nullptr;
	};

} // namespace RHI
} // namespace Lunex
