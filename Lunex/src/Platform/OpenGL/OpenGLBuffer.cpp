#include "stpch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>

namespace Lunex {
	/////////////////////////////////////////////////////
	///////VERTEX BUFFER/////////////////////////////////
	/////////////////////////////////////////////////////
	
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size) : m_Layout({}) {
		LNX_PROFILE_FUNCTION();
		
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}
	
	OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size) : m_Layout({}) {
		LNX_PROFILE_FUNCTION();
		
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
	}
	
	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		LNX_PROFILE_FUNCTION();
		glDeleteBuffers(1, &m_RendererID);
	}
	
	void OpenGLVertexBuffer::Bind() const {
		LNX_PROFILE_FUNCTION();
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}
	
	void OpenGLVertexBuffer::Unbind() const {
		LNX_PROFILE_FUNCTION();
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	
	void OpenGLVertexBuffer::SetData(const void* data, uint32_t size) {
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
	}
	
	/////////////////////////////////////////////////////
	///////INDEX BUFFER//////////////////////////////////
	/////////////////////////////////////////////////////
	
	OpenGLIndexBuffer::OpenGLIndexBuffer(unsigned int* indices, uint32_t count) : m_Count(count) {
		LNX_PROFILE_FUNCTION();
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}
	
	OpenGLIndexBuffer::~OpenGLIndexBuffer() {
		LNX_PROFILE_FUNCTION();
		glDeleteBuffers(1, &m_RendererID);
	}
	
	void OpenGLIndexBuffer::Bind() const {
		LNX_PROFILE_FUNCTION();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}
	
	void OpenGLIndexBuffer::Unbind() const {
		LNX_PROFILE_FUNCTION();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}