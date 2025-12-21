#include "Stpch.h"

#include "Renderer/VertexArray.h"
#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHIVertexArray.h"

namespace Lunex {
	Ref<VertexArray> VertexArray::Create() {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:    
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!"); 
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:  
				return CreateRef<RHI::OpenGLRHIVertexArray>();
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
}