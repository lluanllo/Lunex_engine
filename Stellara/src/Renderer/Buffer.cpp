#include "stpch.h"
#include "Buffer.h"

#include "Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Stellara {
	
	VertexBuffer* Create(float* vertices, uint32_t size) {
		
		switch (Renderer::GetAPI()) {
			case RendererAPI::None:    ST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::OpenGL:  return new OpenGLVertexBuffer(vertices, size);
		}
		
		ST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	IndexBuffer* Create(unsigned int* indices, uint32_t size) {
		
		switch (Renderer::GetAPI()) {
			case RendererAPI::None:    ST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::OpenGL:  return new OpenGLIndexBuffer(indices, size);
		}
		
		ST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}