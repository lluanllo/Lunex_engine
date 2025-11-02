#pragma once

#include "Core/Core.h"
#include "VertexArray.h"

#include "glm/glm.hpp"

namespace Lunex {
	class RendererAPI {
		public:
			enum class API {
				None = 0, OpenGL = 1
			};

			enum class DepthFunc {
				Less = 0,
				LessOrEqual,
				Greater,
				Always
			};

			enum class CullMode {
				None = 0,
				Front,
				Back,
				FrontAndBack
			};

			// Simple blend factors supported by renderer abstraction
			enum class BlendFactor {
				SrcAlpha = 0,
				OneMinusSrcAlpha = 1,
				One = 2,
				Zero = 3
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

			// Depth testing
			virtual void SetDepthTest(bool enabled) = 0;
			virtual void SetDepthFunc(DepthFunc func) = 0;
			// Control whether depth writes are enabled (depth mask)
			virtual void SetDepthMask(bool enabled) = 0;

			// Face culling
			virtual void SetCullMode(CullMode mode) = 0;

			// Blending
			virtual void SetBlend(bool enabled) = 0;
			virtual void SetBlendFunc(BlendFactor src, BlendFactor dst) = 0;
			
			inline static API GetAPI() { return s_API; }
			
	    private:
			static API s_API;
	};
}