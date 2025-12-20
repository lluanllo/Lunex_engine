#include "Stpch.h"

#include "Renderer/VertexArray.h"
#include "Renderer.h"
#include "RHI/OpenGL/OpenGLRHIVertexArray.h"

namespace Lunex {
	Ref<VertexArray> VertexArray::Create() {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				return CreateRef<RHI::OpenGLRHIVertexArray>();
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}