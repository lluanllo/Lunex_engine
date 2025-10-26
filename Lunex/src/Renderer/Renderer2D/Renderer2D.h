#pragma once

#include "Core/Core.h"

#include "Renderer/Camera.h"
#include "Renderer/CameraTypes/OrthographicCamera.h"
#include "Renderer/CameraTypes/EditorCamera.h"
#include "Renderer/Texture.h"
#include "Renderer/Renderer2D/RendererPipeline2D.h"

#include <glm/glm.hpp>

namespace Lunex {
	class Renderer2D {
		public:
			static void Init();
			static void Shutdown();
			
			static void BeginScene(const OrthographicCamera& camera);
			static void BeginScene(const glm::mat4& viewProjection);
			static void BeginScene(const Camera& camera, const glm::mat4& viewMatrix);
			static void BeginScene(const EditorCamera& camera);
			static void EndScene();
			
			// --- Primitives ---
			static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
			static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
			
			static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
			static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
			
			static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
			static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
			static void DrawRotatedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
			
			static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);
			
			static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID = -1);
			static void SetLineWidth(float width);
			static float GetLineWidth();
			
			// --- Stats ---
			static void ResetStats();
			static RendererPipeline2D::Statistics GetStats();
	};
}