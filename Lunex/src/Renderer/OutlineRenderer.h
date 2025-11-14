#pragma once

#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Framebuffer.h"

#include <glm/glm.hpp>

namespace Lunex {
	class OutlineRenderer {
		public:
			static void Init();
			static void Shutdown();
			
			// Renderiza el outline usando post-procesamiento
			static void RenderOutline(
				const Ref<Framebuffer>& framebuffer,
				int selectedEntityID,
				const glm::vec4& outlineColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f),
				float outlineThickness = 2.0f
			);
			
		private:
			static void CreateQuad();
	};
}