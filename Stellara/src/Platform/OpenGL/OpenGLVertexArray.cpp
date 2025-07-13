#include "stpch.h"
#include "OpenGLVertexArray.h"

#include <glad/glad.h>

namespace Stellara {

	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
			case Stellara::ShaderDataType::Float:    return GL_FLOAT;
			case Stellara::ShaderDataType::Float2:   return GL_FLOAT;
			case Stellara::ShaderDataType::Float3:   return GL_FLOAT;
			case Stellara::ShaderDataType::Float4:   return GL_FLOAT;
			case Stellara::ShaderDataType::Mat3:     return GL_FLOAT;
			case Stellara::ShaderDataType::Mat4:     return GL_FLOAT;
			case Stellara::ShaderDataType::Int:      return GL_INT;
			case Stellara::ShaderDataType::Int2:     return GL_INT;
			case Stellara::ShaderDataType::Int3:     return GL_INT;
			case Stellara::ShaderDataType::Int4:     return GL_INT;
			case Stellara::ShaderDataType::Bool:     return GL_BOOL;
		}

		ST_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

    OpenGLVertexArray::OpenGLVertexArray() {
    }

    void OpenGLVertexArray::Bind() const {
        // OpenGL binding code would go here
	}

    void OpenGLVertexArray::Unbind() const {
        // OpenGL unbinding code would go here
    }

    void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
        m_VertexBuffers.push_back(vertexBuffer);
        // OpenGL code to bind the vertex buffer would go here
    }

    void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
        m_IndexBuffer = indexBuffer;
        // OpenGL code to bind the index buffer would go here
	}
}