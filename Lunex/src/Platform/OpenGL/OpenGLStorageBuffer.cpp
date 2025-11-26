#include "stpch.h"
#include "OpenGLStorageBuffer.h"

#include <glad/glad.h>

namespace Lunex {
	OpenGLStorageBuffer::OpenGLStorageBuffer(uint32_t size, const void* data)
		: m_Size(size) {
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, data, GL_DYNAMIC_DRAW);
	}
	
	OpenGLStorageBuffer::~OpenGLStorageBuffer() {
		glDeleteBuffers(1, &m_RendererID);
	}
	
	void OpenGLStorageBuffer::Bind(uint32_t binding) const {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_RendererID);
	}
	
	void OpenGLStorageBuffer::Unbind() const {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	
	void OpenGLStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}
	
	void OpenGLStorageBuffer::GetData(void* data, uint32_t size, uint32_t offset) const {
		glGetNamedBufferSubData(m_RendererID, offset, size, data);
	}
}
