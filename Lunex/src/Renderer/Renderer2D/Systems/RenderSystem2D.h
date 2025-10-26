#pragma once

#include "Core/Core.h"
#include "Renderer/RenderCore/RenderPipeline.h"
#include "Renderer/RenderCore/RenderContext.h"
#include "Renderer/Renderer2D/RendererPipeline2D.h"

#include <glm/glm.hpp>

namespace Lunex {
	class RenderSystem2D {
	public:
		static void Init();
		static void Shutdown();
		
		static void BeginScene(const glm::mat4& viewProjection);
		static void EndScene();
		
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
		static void DrawSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
		static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);
		static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID = -1);
		
		static void ResetStats();
		static RendererPipeline2D::Statistics GetStats();
		
	private:
		static Scope<RendererPipeline2D> s_Pipeline;
	};
}
