#pragma once

#include "Core/Core.h"
#include "Renderer/GraphicsContext.h"

namespace Lunex {
	class RenderContext {
		public:
			// Initialize with a GraphicsContext instance (created by platform layer)
			static void Init(Scope<GraphicsContext> context);
			static void Shutdown();
			
			static void BeginFrame();
			static void EndFrame();
			
			static void SwapBuffers();
			
		private:
			static Scope<GraphicsContext> s_Context;
	};
}
