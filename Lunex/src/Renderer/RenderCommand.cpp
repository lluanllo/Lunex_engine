#include "stpch.h"

#include "Renderer/RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Lunex {
	
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
	
}
