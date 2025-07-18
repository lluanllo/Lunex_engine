#pragma once

#include "Core/Core.h"
#include "VertexArray.h"

#include "glm/glm.hpp"

namespace Stellara {

	class STELLARA_API RendererAPI {
		public:
			enum class API {
				None = 0, OpenGL = 1
			};
			
		private:
			virtual void SetClearColor(const glm::vec4& color) = 0;
			virtual void Clear() = 0;
			 
			virtual void DrawIndexed(const std::shared_ptr<class VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
			inline static API GetAPI() { return s_API; }
			 
	    private:
			static API s_API;
	};
}