#include "stpch.h"
#include "RenderContext.h"

#include "Renderer/GraphicsContext.h"
#include "Renderer/RenderCommand.h"

namespace Lunex {
	Scope<GraphicsContext> RenderContext::s_Context = nullptr;
	
	void RenderContext::Init(Scope<GraphicsContext> context) {
		LNX_PROFILE_FUNCTION();
		s_Context = std::move(context);
		s_Context->Init();
		RenderCommand::Init();
	}
	
	void RenderContext::Shutdown() {
		LNX_PROFILE_FUNCTION();
		s_Context.reset();
	}
	
	void RenderContext::BeginFrame() {
		// Placeholder for per-frame setup (timers, profiling, etc.)
	}
	
	void RenderContext::EndFrame() {
		// Placeholder for per-frame tear-down
	}
	
	void RenderContext::SwapBuffers() {
	    if (s_Context)
		s_Context->SwapBuffers();
	}
}
