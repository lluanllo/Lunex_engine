#pragma once

#include "Core/Core.h"
#include "RHI/RHITypes.h"

namespace Lunex {
	class GraphicsContext {
		public:
			virtual ~GraphicsContext() = default;
			
			virtual void Init() = 0;
			virtual void SwapBuffers() = 0;
			
			static Scope<GraphicsContext> Create(void* window, RHI::GraphicsAPI api = RHI::GraphicsAPI::OpenGL);
	};
}