#include "stpch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>

namespace Stellara {
	
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size) {
		
		glCreateBuffers(1, &m_RendererID);
		
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}
	
	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		
	}
	
	void OpenGLVertexBuffer::Bind() const {
		
	}
	
	void OpenGLVertexBuffer::Unbind() const {
		
	}
}