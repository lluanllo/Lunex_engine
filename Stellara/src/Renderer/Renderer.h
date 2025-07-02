#pragma once

namespace Stellara {
	
	enum class STELLARA_API RendererAPI {
		
		None = 0, OpenGL = 1
		
	};
	
	class STELLARA_API Renderer {
		
		public:
			inline static RendererAPI GetAPI() { return s_RendererAPI; }
			
		private:
			static RendererAPI s_RendererAPI;
	};
}