#pragma once

/**
 * @file OpenGLRHIVertexArray.h
 * @brief OpenGL implementation of RHI Vertex Array Object (VAO)
 *
 * This encapsulates OpenGL's Vertex Array Object which manages:
 * - Vertex buffer bindings
 * - Index buffer binding
 * - Vertex attribute layout configuration
 */

#include "Renderer/VertexArray.h"
#include <glad/glad.h>

namespace Lunex {
	namespace RHI {

		/**
		 * @class OpenGLRHIVertexArray
		 * @brief OpenGL VAO implementation for the RHI layer
		 */
		class OpenGLRHIVertexArray : public VertexArray {
		public:
			OpenGLRHIVertexArray();
			virtual ~OpenGLRHIVertexArray();

			// ============================================
			// BINDING
			// ============================================

			void Bind() const override;
			void Unbind() const override;

			// ============================================
			// BUFFER MANAGEMENT
			// ============================================

			void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
			void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

			const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
			const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

			// ============================================
			// OPENGL SPECIFIC
			// ============================================

			/**
			 * @brief Get native OpenGL VAO ID
			 */
			GLuint GetRendererID() const { return m_RendererID; }

		private:
			/**
			 * @brief Convert ShaderDataType to OpenGL base type
			 */
			static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type);

		private:
			GLuint m_RendererID = 0;
			uint32_t m_VertexBufferIndex = 0;
			std::vector<Ref<VertexBuffer>> m_VertexBuffers;
			Ref<IndexBuffer> m_IndexBuffer;
		};

	} // namespace RHI
} // namespace Lunex
