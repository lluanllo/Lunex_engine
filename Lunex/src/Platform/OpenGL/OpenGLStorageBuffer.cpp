#include "stpch.h"
#include "OpenGLStorageBuffer.h"

#include <glad/glad.h>

namespace Lunex {
	OpenGLStorageBuffer::OpenGLStorageBuffer(uint32_t size, uint32_t binding) {
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_RendererID);
	}
	
	OpenGLStorageBuffer::~OpenGLStorageBuffer() {
		glDeleteBuffers(1, &m_RendererID);
	}
	
	void OpenGLStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}
}
