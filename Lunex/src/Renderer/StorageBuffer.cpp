#include "stpch.h"
#include "StorageBuffer.h"

#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLStorageBuffer.h"

namespace Lunex {
	Ref<StorageBuffer> StorageBuffer::Create(uint32_t size, const void* data) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				return CreateRef<OpenGLStorageBuffer>(size, data);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
