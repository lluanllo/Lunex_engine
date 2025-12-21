#include "stpch.h"
#include "OpenGLRHIVertexArray.h"

namespace Lunex {
	namespace RHI {

		// ============================================================================
		// UTILITY FUNCTIONS
		// ============================================================================

		GLenum OpenGLRHIVertexArray::ShaderDataTypeToOpenGLBaseType(ShaderDataType type) {
			switch (type) {
			case ShaderDataType::None:   return GL_FLOAT;
			case ShaderDataType::Float:  return GL_FLOAT;
			case ShaderDataType::Float2: return GL_FLOAT;
			case ShaderDataType::Float3: return GL_FLOAT;
			case ShaderDataType::Float4: return GL_FLOAT;
			case ShaderDataType::Int:    return GL_INT;
			case ShaderDataType::Int2:   return GL_INT;
			case ShaderDataType::Int3:   return GL_INT;
			case ShaderDataType::Int4:   return GL_INT;
			case ShaderDataType::Mat3:   return GL_FLOAT;
			case ShaderDataType::Mat4:   return GL_FLOAT;
			case ShaderDataType::Bool:   return GL_BOOL;
			}

			LNX_CORE_ASSERT(false, "Unknown ShaderDataType!");
			return 0;
		}

		// ============================================================================
		// CONSTRUCTOR / DESTRUCTOR
		// ============================================================================

		OpenGLRHIVertexArray::OpenGLRHIVertexArray() {
			LNX_PROFILE_FUNCTION();
			glCreateVertexArrays(1, &m_RendererID);
		}

		OpenGLRHIVertexArray::~OpenGLRHIVertexArray() {
			LNX_PROFILE_FUNCTION();
			if (m_RendererID) {
				glDeleteVertexArrays(1, &m_RendererID);
			}
		}

		// ============================================================================
		// BINDING
		// ============================================================================

		void OpenGLRHIVertexArray::Bind() const {
			LNX_PROFILE_FUNCTION();
			glBindVertexArray(m_RendererID);
		}

		void OpenGLRHIVertexArray::Unbind() const {
			LNX_PROFILE_FUNCTION();
			glBindVertexArray(0);
		}

		// ============================================================================
		// BUFFER MANAGEMENT
		// ============================================================================

		void OpenGLRHIVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) {
			LNX_PROFILE_FUNCTION();

			LNX_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

			glBindVertexArray(m_RendererID);
			vertexBuffer->Bind();

			const auto& layout = vertexBuffer->GetLayout();
			for (const auto& element : layout) {
				switch (element.Type) {
				case ShaderDataType::Float:
				case ShaderDataType::Float2:
				case ShaderDataType::Float3:
				case ShaderDataType::Float4: {
					glEnableVertexAttribArray(m_VertexBufferIndex);
					glVertexAttribPointer(
						m_VertexBufferIndex,
						element.GetComponentCount(),
						ShaderDataTypeToOpenGLBaseType(element.Type),
						element.Normalized ? GL_TRUE : GL_FALSE,
						layout.GetStride(),
						reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset))
					);
					m_VertexBufferIndex++;
					break;
				}
				case ShaderDataType::Int:
				case ShaderDataType::Int2:
				case ShaderDataType::Int3:
				case ShaderDataType::Int4:
				case ShaderDataType::Bool: {
					glEnableVertexAttribArray(m_VertexBufferIndex);
					glVertexAttribIPointer(
						m_VertexBufferIndex,
						element.GetComponentCount(),
						ShaderDataTypeToOpenGLBaseType(element.Type),
						layout.GetStride(),
						reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset))
					);
					m_VertexBufferIndex++;
					break;
				}
				case ShaderDataType::Mat3:
				case ShaderDataType::Mat4: {
					uint8_t count = element.GetComponentCount();
					for (uint8_t i = 0; i < count; i++) {
						glEnableVertexAttribArray(m_VertexBufferIndex);
						glVertexAttribPointer(
							m_VertexBufferIndex,
							count,
							ShaderDataTypeToOpenGLBaseType(element.Type),
							element.Normalized ? GL_TRUE : GL_FALSE,
							layout.GetStride(),
							reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset + sizeof(float) * count * i))
						);
						glVertexAttribDivisor(m_VertexBufferIndex, 1);
						m_VertexBufferIndex++;
					}
					break;
				}
				default:
					LNX_CORE_ASSERT(false, "Unknown ShaderDataType!");
				}
			}

			m_VertexBuffers.push_back(vertexBuffer);
		}

		void OpenGLRHIVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) {
			LNX_PROFILE_FUNCTION();

			glBindVertexArray(m_RendererID);
			indexBuffer->Bind();

			m_IndexBuffer = indexBuffer;
		}

	} // namespace RHI
} // namespace Lunex
