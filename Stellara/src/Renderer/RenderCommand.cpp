#include "stpch.h"

#include "Renderer/RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Stellara {
	
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
	
}
