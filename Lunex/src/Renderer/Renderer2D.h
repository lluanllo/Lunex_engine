#pragma once
#include "Core/Core.h"

#include "Scene/Camera/OrthographicCamera.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/EditorCamera.h"

#include "Scene/Components.h"

#include "Texture.h"

namespace Lunex  {
	class Renderer2D {
		public:
			static void Init();
			static void Shutdown();
			
			static void BeginScene(const Camera& camera, const glm::mat4& transform);
			static void BeginScene(const EditorCamera& camera);
			static void BeginScene(const OrthographicCamera& camera); // TODO: Remove
			static void EndScene();
			static void Flush();
			
			// Primitives
			static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
			static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
			static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
			static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
			
			static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
			static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
			
			static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
			static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
			static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
			static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
			
			static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);
			
			static void DrawLine(const glm::vec3& p0, glm::vec3& p1, const glm::vec4& color, int entityID = -1);
			
			static void DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID = -1);
			static void DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
			
			static void DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID);
			
			// Billboard rendering (always faces camera)
			static void DrawBillboard(const glm::vec3& position, const Ref<Texture2D>& texture, 
									  const glm::vec3& cameraPosition, float size = 0.5f, int entityID = -1);
			
			// Camera Frustum Gizmo
			static void DrawCameraFrustum(const glm::mat4& projection, const glm::mat4& view, const glm::vec4& color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), int entityID = -1);
			
			// ========================================
			// LIGHT GIZMOS
			// ========================================
			static void DrawPointLightGizmo(const glm::vec3& position, float radius, const glm::vec4& color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), int entityID = -1);
			static void DrawDirectionalLightGizmo(const glm::vec3& position, const glm::vec3& direction, const glm::vec4& color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), int entityID = -1);
			static void DrawSpotLightGizmo(const glm::vec3& position, const glm::vec3& direction, float range, float outerConeAngle, const glm::vec4& color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), int entityID = -1);
			
			static float GetLineWidth();
			static void SetLineWidth(float width);
			
			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t QuadCount = 0;
				
				uint32_t GetTotalVertexCount() { return QuadCount * 4; }
				uint32_t GetTotalIndexCount() { return QuadCount * 6; }
			};
			static void ResetStats();
			static Statistics GetStats();
			
		private:
			static void StartBatch();
			static void NextBatch();
	};
}