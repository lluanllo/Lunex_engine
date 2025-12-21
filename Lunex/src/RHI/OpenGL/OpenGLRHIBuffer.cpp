#include "stpch.h"
#include "OpenGLRHIBuffer.h"
#include "OpenGLRHIDevice.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL RHI BUFFER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIBuffer::OpenGLRHIBuffer(const BufferDesc& desc, const void* initialData) {
		m_Desc = desc;
		m_Target = OpenGLBufferUtils::GetBufferTarget(desc.Type);
		
		glCreateBuffers(1, &m_BufferID);
		
		GLenum usage = OpenGLBufferUtils::GetBufferUsage(desc.Usage);
		glNamedBufferData(m_BufferID, desc.Size, initialData, usage);
		
		// Track allocation
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(desc.Size);
		}
	}
	
	OpenGLRHIBuffer::~OpenGLRHIBuffer() {
		if (m_MappedData) {
			Unmap();
		}
		
		if (m_BufferID) {
			// Track deallocation
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(m_Desc.Size);
			}
			glDeleteBuffers(1, &m_BufferID);
		}
	}
	
	void OpenGLRHIBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!m_BufferID) return;
		
		if (offset + size > m_Desc.Size) {
			LNX_LOG_ERROR("OpenGLRHIBuffer::SetData - Out of bounds write!");
			return;
		}
		
		glNamedBufferSubData(m_BufferID, offset, size, data);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->GetMutableStatistics().BufferUploads++;
			device->GetMutableStatistics().BufferBytesUploaded += size;
		}
	}
	
	void OpenGLRHIBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!m_BufferID) return;
		glGetNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	MappedBufferRange OpenGLRHIBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}
	
	MappedBufferRange OpenGLRHIBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range;
		
		if (!m_BufferID || m_MappedData) {
			return range;
		}
		
		GLbitfield flags = OpenGLBufferUtils::GetMapAccess(access);
		if (flags == 0) flags = GL_MAP_WRITE_BIT;
		
		m_MappedData = glMapNamedBufferRange(m_BufferID, offset, size, flags);
		
		if (m_MappedData) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		
		return range;
	}
	
	void OpenGLRHIBuffer::Unmap() {
		if (m_BufferID && m_MappedData) {
			glUnmapNamedBuffer(m_BufferID);
			m_MappedData = nullptr;
		}
	}
	
	void OpenGLRHIBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		if (m_BufferID && m_MappedData) {
			glFlushMappedNamedBufferRange(m_BufferID, offset, size);
		}
	}
	
	void OpenGLRHIBuffer::Bind() const {
		if (m_BufferID) {
			glBindBuffer(m_Target, m_BufferID);
		}
	}
	
	void OpenGLRHIBuffer::Unbind() const {
		glBindBuffer(m_Target, 0);
	}
	
	void OpenGLRHIBuffer::BindToPoint(uint32_t bindingPoint) const {
		if (!m_BufferID) return;
		
		switch (m_Desc.Type) {
			case BufferType::Uniform:
				glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_BufferID);
				break;
			case BufferType::Storage:
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_BufferID);
				break;
			default:
				break;
		}
	}
	
	void OpenGLRHIBuffer::OnDebugNameChanged() {
		if (m_BufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_BUFFER, m_BufferID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI VERTEX BUFFER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIVertexBuffer::OpenGLRHIVertexBuffer(const BufferDesc& desc, const VertexLayout& layout, const void* initialData)
		: m_Layout(layout)
	{
		m_Desc = desc;
		m_Desc.Type = BufferType::Vertex;
		m_Desc.Stride = layout.GetStride();
		
		glCreateBuffers(1, &m_BufferID);
		
		GLenum usage = OpenGLBufferUtils::GetBufferUsage(desc.Usage);
		glNamedBufferData(m_BufferID, desc.Size, initialData, usage);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(desc.Size);
		}
	}
	
	OpenGLRHIVertexBuffer::~OpenGLRHIVertexBuffer() {
		if (m_MappedData) {
			Unmap();
		}
		
		if (m_BufferID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(m_Desc.Size);
			}
			glDeleteBuffers(1, &m_BufferID);
		}
	}
	
	void OpenGLRHIVertexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!m_BufferID) return;
		glNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	void OpenGLRHIVertexBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!m_BufferID) return;
		glGetNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	MappedBufferRange OpenGLRHIVertexBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}
	
	MappedBufferRange OpenGLRHIVertexBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range;
		if (!m_BufferID || m_MappedData) return range;
		
		GLbitfield flags = OpenGLBufferUtils::GetMapAccess(access);
		if (flags == 0) flags = GL_MAP_WRITE_BIT;
		
		m_MappedData = glMapNamedBufferRange(m_BufferID, offset, size, flags);
		if (m_MappedData) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}
	
	void OpenGLRHIVertexBuffer::Unmap() {
		if (m_BufferID && m_MappedData) {
			glUnmapNamedBuffer(m_BufferID);
			m_MappedData = nullptr;
		}
	}
	
	void OpenGLRHIVertexBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		if (m_BufferID && m_MappedData) {
			glFlushMappedNamedBufferRange(m_BufferID, offset, size);
		}
	}
	
	void OpenGLRHIVertexBuffer::Bind() const {
		if (m_BufferID) {
			glBindBuffer(GL_ARRAY_BUFFER, m_BufferID);
		}
	}
	
	void OpenGLRHIVertexBuffer::Unbind() const {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	
	void OpenGLRHIVertexBuffer::BindToPoint(uint32_t bindingPoint) const {
		// Vertex buffers don't use binding points
		Bind();
	}
	
	void OpenGLRHIVertexBuffer::OnDebugNameChanged() {
		if (m_BufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_BUFFER, m_BufferID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI INDEX BUFFER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIIndexBuffer::OpenGLRHIIndexBuffer(const BufferDesc& desc, const void* initialData) {
		m_Desc = desc;
		m_Desc.Type = BufferType::Index;
		
		glCreateBuffers(1, &m_BufferID);
		
		GLenum usage = OpenGLBufferUtils::GetBufferUsage(desc.Usage);
		glNamedBufferData(m_BufferID, desc.Size, initialData, usage);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(desc.Size);
		}
	}
	
	OpenGLRHIIndexBuffer::~OpenGLRHIIndexBuffer() {
		if (m_MappedData) Unmap();
		if (m_BufferID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(m_Desc.Size);
			}
			glDeleteBuffers(1, &m_BufferID);
		}
	}
	
	void OpenGLRHIIndexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!m_BufferID) return;
		glNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	void OpenGLRHIIndexBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!m_BufferID) return;
		glGetNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	MappedBufferRange OpenGLRHIIndexBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}
	
	MappedBufferRange OpenGLRHIIndexBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range;
		if (!m_BufferID || m_MappedData) return range;
		
		GLbitfield flags = OpenGLBufferUtils::GetMapAccess(access);
		if (flags == 0) flags = GL_MAP_WRITE_BIT;
		
		m_MappedData = glMapNamedBufferRange(m_BufferID, offset, size, flags);
		if (m_MappedData) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}
	
	void OpenGLRHIIndexBuffer::Unmap() {
		if (m_BufferID && m_MappedData) {
			glUnmapNamedBuffer(m_BufferID);
			m_MappedData = nullptr;
		}
	}
	
	void OpenGLRHIIndexBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		if (m_BufferID && m_MappedData) {
			glFlushMappedNamedBufferRange(m_BufferID, offset, size);
		}
	}
	
	void OpenGLRHIIndexBuffer::Bind() const {
		if (m_BufferID) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BufferID);
	}
	
	void OpenGLRHIIndexBuffer::Unbind() const {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	void OpenGLRHIIndexBuffer::BindToPoint(uint32_t bindingPoint) const {
		Bind();
	}
	
	void OpenGLRHIIndexBuffer::OnDebugNameChanged() {
		if (m_BufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_BUFFER, m_BufferID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI UNIFORM BUFFER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIUniformBuffer::OpenGLRHIUniformBuffer(const BufferDesc& desc, const void* initialData) {
		m_Desc = desc;
		m_Desc.Type = BufferType::Uniform;
		
		glCreateBuffers(1, &m_BufferID);
		
		GLenum usage = OpenGLBufferUtils::GetBufferUsage(desc.Usage);
		glNamedBufferData(m_BufferID, desc.Size, initialData, usage);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(desc.Size);
		}
	}
	
	OpenGLRHIUniformBuffer::~OpenGLRHIUniformBuffer() {
		if (m_MappedData) Unmap();
		if (m_BufferID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(m_Desc.Size);
			}
			glDeleteBuffers(1, &m_BufferID);
		}
	}
	
	void OpenGLRHIUniformBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!m_BufferID) return;
		glNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	void OpenGLRHIUniformBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!m_BufferID) return;
		glGetNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	MappedBufferRange OpenGLRHIUniformBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}
	
	MappedBufferRange OpenGLRHIUniformBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range;
		if (!m_BufferID || m_MappedData) return range;
		
		GLbitfield flags = OpenGLBufferUtils::GetMapAccess(access);
		if (flags == 0) flags = GL_MAP_WRITE_BIT;
		
		m_MappedData = glMapNamedBufferRange(m_BufferID, offset, size, flags);
		if (m_MappedData) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}
	
	void OpenGLRHIUniformBuffer::Unmap() {
		if (m_BufferID && m_MappedData) {
			glUnmapNamedBuffer(m_BufferID);
			m_MappedData = nullptr;
		}
	}
	
	void OpenGLRHIUniformBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		if (m_BufferID && m_MappedData) {
			glFlushMappedNamedBufferRange(m_BufferID, offset, size);
		}
	}
	
	void OpenGLRHIUniformBuffer::Bind() const {
		if (m_BufferID) glBindBuffer(GL_UNIFORM_BUFFER, m_BufferID);
	}
	
	void OpenGLRHIUniformBuffer::Unbind() const {
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	
	void OpenGLRHIUniformBuffer::BindToPoint(uint32_t bindingPoint) const {
		Bind(bindingPoint);
	}
	
	void OpenGLRHIUniformBuffer::Bind(uint32_t bindingPoint) const {
		if (m_BufferID) {
			glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_BufferID);
		}
	}
	
	void OpenGLRHIUniformBuffer::OnDebugNameChanged() {
		if (m_BufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_BUFFER, m_BufferID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI STORAGE BUFFER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIStorageBuffer::OpenGLRHIStorageBuffer(const BufferDesc& desc, const void* initialData) {
		m_Desc = desc;
		m_Desc.Type = BufferType::Storage;
		
		glCreateBuffers(1, &m_BufferID);
		
		GLenum usage = OpenGLBufferUtils::GetBufferUsage(desc.Usage);
		glNamedBufferData(m_BufferID, desc.Size, initialData, usage);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(desc.Size);
		}
	}
	
	OpenGLRHIStorageBuffer::~OpenGLRHIStorageBuffer() {
		if (m_MappedData) Unmap();
		if (m_BufferID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(m_Desc.Size);
			}
			glDeleteBuffers(1, &m_BufferID);
		}
	}
	
	void OpenGLRHIStorageBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!m_BufferID) return;
		glNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	void OpenGLRHIStorageBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!m_BufferID) return;
		glGetNamedBufferSubData(m_BufferID, offset, size, data);
	}
	
	MappedBufferRange OpenGLRHIStorageBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}
	
	MappedBufferRange OpenGLRHIStorageBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range;
		if (!m_BufferID || m_MappedData) return range;
		
		GLbitfield flags = OpenGLBufferUtils::GetMapAccess(access);
		if (flags == 0) flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
		
		m_MappedData = glMapNamedBufferRange(m_BufferID, offset, size, flags);
		if (m_MappedData) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}
	
	void OpenGLRHIStorageBuffer::Unmap() {
		if (m_BufferID && m_MappedData) {
			glUnmapNamedBuffer(m_BufferID);
			m_MappedData = nullptr;
		}
	}
	
	void OpenGLRHIStorageBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		if (m_BufferID && m_MappedData) {
			glFlushMappedNamedBufferRange(m_BufferID, offset, size);
		}
	}
	
	void OpenGLRHIStorageBuffer::Bind() const {
		if (m_BufferID) glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
	}
	
	void OpenGLRHIStorageBuffer::Unbind() const {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	
	void OpenGLRHIStorageBuffer::BindToPoint(uint32_t bindingPoint) const {
		BindForCompute(bindingPoint);
	}
	
	void OpenGLRHIStorageBuffer::BindForCompute(uint32_t bindingPoint) const {
		if (m_BufferID) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_BufferID);
		}
	}
	
	void OpenGLRHIStorageBuffer::BindForRead(uint32_t bindingPoint) const {
		BindForCompute(bindingPoint);
	}
	
	void OpenGLRHIStorageBuffer::OnDebugNameChanged() {
		if (m_BufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_BUFFER, m_BufferID, -1, m_DebugName.c_str());
		}
	}

} // namespace RHI
} // namespace Lunex
