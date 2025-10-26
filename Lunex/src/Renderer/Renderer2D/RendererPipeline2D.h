#pragma once

#include "Core/Core.h"
#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/Buffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/RenderCore/RenderCommand.h"

#include <glm/glm.hpp>
#include <array>

namespace Lunex {
	struct QuadVertex {
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;
		int EntityID;
	};
	
	struct CircleVertex {
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;
		int EntityID;
	};
	
	struct LineVertex {
		glm::vec3 Position;
		glm::vec4 Color;
		int EntityID;
	};
	
	class RendererPipeline2D {
		public:
			static void Init();
			static void Shutdown();
			
			// Scene control
			static void BeginScene(const glm::mat4& viewProjection);
			static void EndScene();
			
			// Submissions (API used by Renderer2D facade)
			static void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
			static void SubmitSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
			static void SubmitRotatedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
			
			static void SubmitCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);
			
			static void SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID = -1);
			
			// Stats
			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t QuadCount = 0;
				
				uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
				uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
			};
			
			static void ResetStats();
			static Statistics GetStats();
			
		private:
			// internal helpers
			static void StartBatch();
			static void NextBatch();
			static void Flush();
			
		private:
			struct PipelineData {
				// limits
				static const uint32_t MaxQuads = 20000;
				static const uint32_t MaxVertices = MaxQuads * 4;
				static const uint32_t MaxIndices = MaxQuads * 6;
				static const uint32_t MaxTextureSlots = 32;
				
				// quad
				Ref<VertexArray> QuadVertexArray;
				Ref<VertexBuffer> QuadVertexBuffer;
				uint32_t QuadIndexCount = 0;
				QuadVertex* QuadVertexBufferBase = nullptr;
				QuadVertex* QuadVertexBufferPtr = nullptr;
				
				// circle
				Ref<VertexArray> CircleVertexArray;
				Ref<VertexBuffer> CircleVertexBuffer;
				uint32_t CircleIndexCount = 0;
				CircleVertex* CircleVertexBufferBase = nullptr;
				CircleVertex* CircleVertexBufferPtr = nullptr;
				
				// lines
				Ref<VertexArray> LineVertexArray;
				Ref<VertexBuffer> LineVertexBuffer;
				uint32_t LineVertexCount = 0;
				LineVertex* LineVertexBufferBase = nullptr;
				LineVertex* LineVertexBufferPtr = nullptr;
				
				// textures
				std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
				uint32_t TextureSlotIndex = 1; // 0 = white texture
				
				// shaders
				Ref<Shader> QuadShader;
				Ref<Shader> CircleShader;
				Ref<Shader> LineShader;
				
				Ref<Texture2D> WhiteTexture;
				
				// quad vertex positions (object-space)
				glm::vec4 QuadVertexPositions[4];
				
				// camera uniform
				struct CameraData {
					glm::mat4 ViewProjection;
				} CameraBuffer;
				Ref<UniformBuffer> CameraUniformBuffer;
				
				// stats
				Statistics Stats;
				
				// line width
				float LineWidth = 2.0f;
			};
			
			static PipelineData s_Data;
	};
}