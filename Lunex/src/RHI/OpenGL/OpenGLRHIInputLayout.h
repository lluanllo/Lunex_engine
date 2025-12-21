#pragma once

/**
 * @file OpenGLRHIInputLayout.h
 * @brief OpenGL implementation of RHI Input Layout
 * 
 * Internally uses VAO (Vertex Array Object) to emulate the InputLayout concept.
 * This is OpenGL-specific - other backends handle this differently.
 */

#include "RHI/RHIInputLayout.h"
#include <glad/glad.h>

namespace Lunex {
namespace RHI {

	/**
	 * @class OpenGLRHIInputLayout
	 * @brief OpenGL implementation of RHIInputLayout
	 */
	class OpenGLRHIInputLayout : public RHIInputLayout {
	public:
		OpenGLRHIInputLayout(const InputLayoutDesc& desc);
		virtual ~OpenGLRHIInputLayout();
		
		// ============================================
		// RHIResource interface
		// ============================================
		
		ResourceType GetResourceType() const override { return ResourceType::InputLayout; }
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_VAO); }
		bool IsValid() const override { return m_VAO != 0; }
		
		// ============================================
		// RHIInputLayout interface
		// ============================================
		
		const InputLayoutDesc& GetDescription() const override { return m_Desc; }
		uint32_t GetNumInputSlots() const override { return m_NumSlots; }
		uint32_t GetStride(uint32_t slot) const override;
		
		// ============================================
		// OpenGL-specific
		// ============================================
		
		/**
		 * @brief Apply this input layout with the given buffers
		 * Creates/updates internal VAO
		 */
		void Apply(const std::vector<VertexBufferView>& vertexBuffers, 
				   const IndexBufferView* indexBuffer = nullptr);
		
		/**
		 * @brief Bind the internal VAO
		 */
		void Bind() const;
		
		/**
		 * @brief Unbind (bind VAO 0)
		 */
		void Unbind() const;
		
		/**
		 * @brief Get the internal VAO ID (for debugging)
		 */
		GLuint GetVAO() const { return m_VAO; }
		
	private:
		/**
		 * @brief Convert DataType to OpenGL type enum
		 */
		static GLenum DataTypeToGLType(DataType type);
		
		/**
		 * @brief Check if DataType is normalized integer
		 */
		static bool IsNormalizedType(DataType type);
		
		/**
		 * @brief Check if DataType is integer
		 */
		static bool IsIntegerType(DataType type);
		
	private:
		InputLayoutDesc m_Desc;
		GLuint m_VAO = 0;
		uint32_t m_NumSlots = 0;
		std::vector<uint32_t> m_Strides;
		
		// Cache bound buffers to avoid redundant state changes
		std::vector<GLuint> m_BoundVertexBuffers;
		GLuint m_BoundIndexBuffer = 0;
	};

} // namespace RHI
} // namespace Lunex
