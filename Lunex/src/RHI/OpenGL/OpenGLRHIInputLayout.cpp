#include "stpch.h"
#include "OpenGLRHIInputLayout.h"
#include "OpenGLRHIBuffer.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// CONSTRUCTOR / DESTRUCTOR
	// ============================================================================
	
	OpenGLRHIInputLayout::OpenGLRHIInputLayout(const InputLayoutDesc& desc)
		: m_Desc(desc)
	{
		// Calculate number of slots and strides
		m_NumSlots = 0;
		for (const auto& element : desc.Elements) {
			m_NumSlots = std::max(m_NumSlots, element.InputSlot + 1);
		}
		
		m_Strides.resize(m_NumSlots, 0);
		for (uint32_t slot = 0; slot < m_NumSlots; slot++) {
			m_Strides[slot] = desc.GetStride(slot);
		}
		
		// Create VAO
		glCreateVertexArrays(1, &m_VAO);
		
		if (!desc.DebugName.empty()) {
			glObjectLabel(GL_VERTEX_ARRAY, m_VAO, -1, desc.DebugName.c_str());
		}
		
		// Configure vertex attributes (without binding buffers yet)
		// Note: In modern OpenGL (4.5+), we can use DSA to configure without binding
		for (size_t i = 0; i < desc.Elements.size(); i++) {
			const auto& element = desc.Elements[i];
			uint32_t attribIndex = static_cast<uint32_t>(i);
			
			glEnableVertexArrayAttrib(m_VAO, attribIndex);
			
			// Set binding index (maps attribute to buffer slot)
			glVertexArrayAttribBinding(m_VAO, attribIndex, element.InputSlot);
			
			// Set attribute format
			GLenum glType = DataTypeToGLType(element.Format);
			uint32_t componentCount = GetDataTypeComponentCount(element.Format);
			
			if (IsIntegerType(element.Format)) {
				glVertexArrayAttribIFormat(m_VAO, attribIndex, componentCount, glType, element.AlignedByteOffset);
			} else {
				GLboolean normalized = IsNormalizedType(element.Format) ? GL_TRUE : GL_FALSE;
				glVertexArrayAttribFormat(m_VAO, attribIndex, componentCount, glType, normalized, element.AlignedByteOffset);
			}
			
			// Set per-instance or per-vertex
			if (element.IsPerInstance) {
				glVertexArrayBindingDivisor(m_VAO, element.InputSlot, element.InstanceDataStepRate > 0 ? element.InstanceDataStepRate : 1);
			}
		}
		
		LNX_LOG_INFO("Created OpenGL InputLayout with {0} attributes across {1} slots", 
			desc.Elements.size(), m_NumSlots);
	}
	
	OpenGLRHIInputLayout::~OpenGLRHIInputLayout() {
		if (m_VAO) {
			glDeleteVertexArrays(1, &m_VAO);
		}
	}
	
	// ============================================================================
	// RHIInputLayout INTERFACE
	// ============================================================================
	
	uint32_t OpenGLRHIInputLayout::GetStride(uint32_t slot) const {
		if (slot < m_Strides.size()) {
			return m_Strides[slot];
		}
		return 0;
	}
	
	// ============================================================================
	// OPENGL-SPECIFIC
	// ============================================================================
	
	void OpenGLRHIInputLayout::Apply(const std::vector<VertexBufferView>& vertexBuffers, 
									  const IndexBufferView* indexBuffer) {
		if (!m_VAO) return;
		
		// Bind vertex buffers to slots using DSA
		for (size_t slot = 0; slot < vertexBuffers.size() && slot < m_NumSlots; slot++) {
			const auto& view = vertexBuffers[slot];
			if (!view.Buffer) continue;
			
			// Get native OpenGL buffer ID
			GLuint bufferID = 0;
			if (auto* glBuffer = dynamic_cast<OpenGLRHIBuffer*>(view.Buffer.get())) {
				bufferID = glBuffer->GetBufferID();
			}
			
			// Only update if changed
			if (slot >= m_BoundVertexBuffers.size()) {
				m_BoundVertexBuffers.resize(slot + 1, 0);
			}
			
			if (m_BoundVertexBuffers[slot] != bufferID) {
				glVertexArrayVertexBuffer(m_VAO, static_cast<GLuint>(slot), bufferID, 
					static_cast<GLintptr>(view.Offset), static_cast<GLsizei>(view.Stride));
				m_BoundVertexBuffers[slot] = bufferID;
			}
		}
		
		// Bind index buffer if provided
		if (indexBuffer && indexBuffer->Buffer) {
			GLuint indexBufferID = 0;
			if (auto* glBuffer = dynamic_cast<OpenGLRHIBuffer*>(indexBuffer->Buffer.get())) {
				indexBufferID = glBuffer->GetBufferID();
			}
			
			if (m_BoundIndexBuffer != indexBufferID) {
				glVertexArrayElementBuffer(m_VAO, indexBufferID);
				m_BoundIndexBuffer = indexBufferID;
			}
		}
	}
	
	void OpenGLRHIInputLayout::Bind() const {
		glBindVertexArray(m_VAO);
	}
	
	void OpenGLRHIInputLayout::Unbind() const {
		glBindVertexArray(0);
	}
	
	// ============================================================================
	// HELPERS
	// ============================================================================
	
	GLenum OpenGLRHIInputLayout::DataTypeToGLType(DataType type) {
		switch (type) {
			case DataType::Float:
			case DataType::Float2:
			case DataType::Float3:
			case DataType::Float4:
			case DataType::Mat3:
			case DataType::Mat4:
				return GL_FLOAT;
				
			case DataType::Int:
			case DataType::Int2:
			case DataType::Int3:
			case DataType::Int4:
				return GL_INT;
				
			case DataType::UInt:
			case DataType::UInt2:
			case DataType::UInt3:
			case DataType::UInt4:
				return GL_UNSIGNED_INT;
				
			case DataType::Bool:
				return GL_BOOL;
				
			default:
				return GL_FLOAT;
		}
	}
	
	bool OpenGLRHIInputLayout::IsNormalizedType(DataType type) {
		// For now, no types are normalized. Could extend for UNORM types.
		return false;
	}
	
	bool OpenGLRHIInputLayout::IsIntegerType(DataType type) {
		switch (type) {
			case DataType::Int:
			case DataType::Int2:
			case DataType::Int3:
			case DataType::Int4:
			case DataType::UInt:
			case DataType::UInt2:
			case DataType::UInt3:
			case DataType::UInt4:
				return true;
			default:
				return false;
		}
	}
	
	// ============================================================================
	// FACTORY
	// ============================================================================
	
	Ref<RHIInputLayout> RHIInputLayout::Create(const InputLayoutDesc& desc) {
		return CreateRef<OpenGLRHIInputLayout>(desc);
	}

} // namespace RHI
} // namespace Lunex
