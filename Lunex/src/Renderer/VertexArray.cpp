#include "Stpch.h"

#include "Renderer/VertexArray.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"


namespace Lunex {
	Ref<VertexArray> VertexArray::Create() {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope <OpenGLVertexArray>();
		}
		
		LN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}