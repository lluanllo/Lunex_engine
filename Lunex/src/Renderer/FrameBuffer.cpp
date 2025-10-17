#include "stpch.h"
#include "FrameBuffer.h"

#include "Renderer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Lunex {
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}