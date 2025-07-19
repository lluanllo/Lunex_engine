#pragma once

#include "RenderCommand.h"

namespace Stellara {
	 
	class STELLARA_API Renderer {
		
		public:
			static void BeginScene();
			static void EndScene();
			
			static void Submit(const std::shared_ptr<VertexArray>& vertexArray);
			
			inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	};
}