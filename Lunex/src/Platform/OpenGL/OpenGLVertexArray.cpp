#include "stpch.h"
#include "OpenGLVertexArray.h"

#include <glad/glad.h>

namespace Lunex {
	
	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type) {
		
		switch (type) {
			case Lunex::ShaderDataType::None:   return GL_FLOAT;
			case Lunex::ShaderDataType::Float:  return GL_FLOAT;
			case Lunex::ShaderDataType::Float2: return GL_FLOAT;
			case Lunex::ShaderDataType::Float3: return GL_FLOAT;
			case Lunex::ShaderDataType::Float4: return GL_FLOAT;
			case Lunex::ShaderDataType::Int:    return GL_INT;
			case Lunex::ShaderDataType::Int2:   return GL_INT;
			case Lunex::ShaderDataType::Int3:   return GL_INT;
			case Lunex::ShaderDataType::Int4:   return GL_INT;
			case Lunex::ShaderDataType::Mat3:   return GL_FLOAT;
			case Lunex::ShaderDataType::Mat4:   return GL_FLOAT;
			case Lunex::ShaderDataType::Bool:   return GL_BOOL;
		}
		
		LN_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}
	
	OpenGLVertexArray::OpenGLVertexArray() {
		LNX_PROFILE_FUNCTION();
		glCreateVertexArrays(1, &m_RendererID);
	}
	
	OpenGLVertexArray::~OpenGLVertexArray() {
		LNX_PROFILE_FUNCTION();
		glDeleteVertexArrays(1, &m_RendererID);
	}
	
	void OpenGLVertexArray::Bind() const {
		LNX_PROFILE_FUNCTION();
		glBindVertexArray(m_RendererID);
	}
	
	void OpenGLVertexArray::Unbind() const {
		LNX_PROFILE_FUNCTION();
		glBindVertexArray(0);
	}
	
	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) {
		LNX_PROFILE_FUNCTION();
		LN_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");
		
		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();
		
		uint32_t index = 0;
		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				(const void*)element.Offset);
			index++;
		}
		m_VertexBuffers.push_back(vertexBuffer);
	}
	
	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) {
		LNX_PROFILE_FUNCTION();
		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();
		
		m_IndexBuffer = indexBuffer;
	}

}