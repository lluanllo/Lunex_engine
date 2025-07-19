#include "Stpch.h"

#include "Renderer/VertexArray.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"


namespace Stellara {
	
	VertexArray* VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    ST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return new OpenGLVertexArray();
		}

		ST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}