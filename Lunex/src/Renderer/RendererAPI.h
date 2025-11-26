#pragma once

#include "Core/Core.h"
#include "VertexArray.h"

#include "glm/glm.hpp"

namespace Lunex {
	class   RendererAPI {
		public:
			enum class API {
				None = 0, OpenGL = 1
			};
			
			enum class DepthFunc {
				Less = 0,
				LessEqual = 1,
				Equal = 2,
				Greater = 3,
				GreaterEqual = 4,
				Always = 5,
				Never = 6
			};
			
			enum class CullMode {
				None = 0,
				Front = 1,
				Back = 2,
				FrontAndBack = 3
			};
			
		public:
			virtual ~RendererAPI() = default;
			
			virtual void Init() = 0;
			virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
			virtual void SetClearColor(const glm::vec4& color) = 0;
			virtual void Clear() = 0;
			
			virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
			virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
			
			virtual void SetLineWidth(float width) = 0;
			virtual void SetDepthMask(bool enabled) = 0;
			virtual void SetDepthFunc(DepthFunc func) = 0;
			virtual void SetCullMode(CullMode mode) = 0;
			
			inline static API GetAPI() { return s_API; }
			
	    private:
			static API s_API;
	};
}