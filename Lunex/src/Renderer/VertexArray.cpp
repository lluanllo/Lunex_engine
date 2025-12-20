#include "Stpch.h"

#include "Renderer/VertexArray.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Lunex {
	Ref<VertexArray> VertexArray::Create() {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				// Note: VertexArray is kept as-is for now since it mainly manages GL state
				// It will be migrated when we implement a full RHI pipeline state object
				return CreateRef<OpenGLVertexArray>();
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}