#include "stpch.h"
#include "RendererPipeline2D.h"

#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Texture.h"
#include "Renderer/Shader.h"
#include "Renderer/RenderCore/RenderCommand.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
	// Initialize static
	RendererPipeline2D::PipelineData RendererPipeline2D::s_Data;
	
	RendererPipeline2D::RendererPipeline2D() = default;
	
	// RenderPipeline interface implementations
	void RendererPipeline2D::BeginFrame() {
		// No-op for 2D renderer
	}
	
	void RendererPipeline2D::EndFrame() {
		// No-op for 2D renderer
	}
	
	void RendererPipeline2D::Submit(const Ref<VertexArray>& vertexArray, const Ref<Shader>& shader, const glm::mat4& transform) {
		// Not implemented for 2D renderer
	}
	
	// -------------------------------------------------------------------------
	// Init / Shutdown
	// -------------------------------------------------------------------------
	void RendererPipeline2D::Init() {
		LNX_PROFILE_FUNCTION();
		
		// --- Quad setup ---
		s_Data.QuadVertexArray = VertexArray::Create();
		s_Data.QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
		s_Data.QuadVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"     },
			{ ShaderDataType::Float4, "a_Color"        },
			{ ShaderDataType::Float2, "a_TexCoord"     },
			{ ShaderDataType::Float,  "a_TexIndex"     },
			{ ShaderDataType::Float,  "a_TilingFactor" },
			{ ShaderDataType::Int,    "a_EntityID"     }
		});
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);
		
		// Pre-allocate CPU-side buffer
		s_Data.QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];
		
		// Index buffer
		uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6) {
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;
			
			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;
			
			offset += 4;
		}
		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIB);
		delete[] quadIndices;
		
		// --- Line setup ---
		s_Data.LineVertexArray = VertexArray::Create();
		s_Data.LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
		s_Data.LineVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Int,    "a_EntityID" }
		});
		s_Data.LineVertexArray->AddVertexBuffer(s_Data.LineVertexBuffer);
		s_Data.LineVertexBufferBase = new LineVertex[s_Data.MaxVertices];
		
		// --- Circle setup (uses same index buffer as quads) ---
		s_Data.CircleVertexArray = VertexArray::Create();
		s_Data.CircleVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(CircleVertex));
		s_Data.CircleVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_WorldPosition" },
			{ ShaderDataType::Float3, "a_LocalPosition" },
			{ ShaderDataType::Float4, "a_Color"         },
			{ ShaderDataType::Float,  "a_Thickness"     },
			{ ShaderDataType::Float,  "a_Fade"          },
			{ ShaderDataType::Int,    "a_EntityID"      }
		});
		s_Data.CircleVertexArray->AddVertexBuffer(s_Data.CircleVertexBuffer);
		s_Data.CircleVertexArray->SetIndexBuffer(quadIB); // reuse
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
		
		// White texture (slot 0)
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t white = 0xffffffff;
		s_Data.WhiteTexture->SetData(&white, sizeof(uint32_t));
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;
		
		// Load shaders
		s_Data.QuadShader = Shader::Create("assets/shaders/Renderer2D_Quad.glsl");
		s_Data.CircleShader = Shader::Create("assets/shaders/Renderer2D_Circle.glsl");
		s_Data.LineShader = Shader::Create("assets/shaders/Renderer2D_Line.glsl");
		
		// Set quad positions (object local)
		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };
		
		// Camera UBO
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(RendererPipeline2D::PipelineData::CameraData), 0);
		
		// Fill texture samplers in shader (only if shader expects them)
		if (s_Data.QuadShader) {
			s_Data.QuadShader->Bind();
			int samplers[s_Data.MaxTextureSlots];
			for (uint32_t i = 0; i < s_Data.MaxTextureSlots; i++) samplers[i] = i;
			s_Data.QuadShader->SetIntArray("u_Textures", samplers, s_Data.MaxTextureSlots);
		}
		
		StartBatch();
	}
	
	void RendererPipeline2D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		delete[] s_Data.QuadVertexBufferBase;
		delete[] s_Data.CircleVertexBufferBase;
		delete[] s_Data.LineVertexBufferBase;
	}
	
	// -------------------------------------------------------------------------
	// Scene Control
	// -------------------------------------------------------------------------
	void RendererPipeline2D::BeginScene(const glm::mat4& viewProjection) {
		LNX_PROFILE_FUNCTION();
		s_Data.CameraBuffer.ViewProjection = viewProjection;
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(s_Data.CameraBuffer));
		StartBatch();
	}
	
	void RendererPipeline2D::EndScene() {
		LNX_PROFILE_FUNCTION();
		Flush();
	}
	
	// -------------------------------------------------------------------------
	// Submissions
	// -------------------------------------------------------------------------
	void RendererPipeline2D::SubmitQuad(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 texCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float texIndex = 0.0f;
		const float tiling = 1.0f;
		
		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();
		
		for (size_t i = 0; i < quadVertexCount; i++) {
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tiling;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}
		
		s_Data.QuadIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}
	
	void RendererPipeline2D::SubmitSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 texCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		
		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();
		
		float textureIndex = 0.0f;
		
		if (texture) {
			// Find existing slot
			for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++) {
				if (*s_Data.TextureSlots[i] == *texture) {
					textureIndex = (float)i;
					break;
				}
			}
			
			// New slot
			if (textureIndex == 0.0f) {
				if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
					NextBatch();
				
				textureIndex = (float)s_Data.TextureSlotIndex;
				s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
				s_Data.TextureSlotIndex++;
			}
		}
		
		for (size_t i = 0; i < quadVertexCount; i++) {
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}
		
		s_Data.QuadIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}
	
	void RendererPipeline2D::SubmitRotatedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		// same as SubmitSprite - kept for API parity
		SubmitSprite(transform, texture, tilingFactor, tintColor, entityID);
	}
	
	void RendererPipeline2D::SubmitCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Data.CircleIndexCount >= s_Data.MaxIndices)
			NextBatch();
		
		for (size_t i = 0; i < 4; i++) {
			s_Data.CircleVertexBufferPtr->WorldPosition = transform * s_Data.QuadVertexPositions[i];
			s_Data.CircleVertexBufferPtr->LocalPosition = s_Data.QuadVertexPositions[i] * 2.0f;
			s_Data.CircleVertexBufferPtr->Color = color;
			s_Data.CircleVertexBufferPtr->Thickness = thickness;
			s_Data.CircleVertexBufferPtr->Fade = fade;
			s_Data.CircleVertexBufferPtr->EntityID = entityID;
			s_Data.CircleVertexBufferPtr++;
		}
		
		s_Data.CircleIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}
	
	void RendererPipeline2D::SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Data.LineVertexCount + 2 >= s_Data.MaxVertices)
			NextBatch();
		
		s_Data.LineVertexBufferPtr->Position = p0;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;
		
		s_Data.LineVertexBufferPtr->Position = p1;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;
		
		s_Data.LineVertexCount += 2;
	}
	
	// -------------------------------------------------------------------------
	// Batch management
	// -------------------------------------------------------------------------
	void RendererPipeline2D::StartBatch() {
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
		
		s_Data.CircleIndexCount = 0;
		s_Data.CircleVertexBufferPtr = s_Data.CircleVertexBufferBase;
		
		s_Data.LineVertexCount = 0;
		s_Data.LineVertexBufferPtr = s_Data.LineVertexBufferBase;
		
		s_Data.TextureSlotIndex = 1;
	}
	
	void RendererPipeline2D::NextBatch() {
		Flush();
		StartBatch();
	}
	
	void RendererPipeline2D::Flush() {
		LNX_PROFILE_FUNCTION();
		
		// --- Quads ---
		if (s_Data.QuadIndexCount && s_Data.QuadShader) {
			uint32_t size = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, size);
			
			// bind textures
			for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++) {
				s_Data.TextureSlots[i]->Bind(i);
			}
			
			// set camera UBO if needed (already uploaded in BeginScene)
			s_Data.QuadShader->Bind();
			s_Data.QuadShader->SetMat4("u_ViewProjection", s_Data.CameraBuffer.ViewProjection);
			
			s_Data.QuadVertexArray->Bind();
			RenderCommand::DrawIndexed(s_Data.QuadVertexArray, s_Data.QuadIndexCount);
			s_Data.Stats.DrawCalls++;
		}
		
		// --- Circles ---
		if (s_Data.CircleIndexCount && s_Data.CircleShader) {
			uint32_t size = (uint32_t)((uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase);
			s_Data.CircleVertexBuffer->SetData(s_Data.CircleVertexBufferBase, size);
			
			s_Data.CircleShader->Bind();
			s_Data.CircleShader->SetMat4("u_ViewProjection", s_Data.CameraBuffer.ViewProjection);
			
			s_Data.CircleVertexArray->Bind();
			RenderCommand::DrawIndexed(s_Data.CircleVertexArray, s_Data.CircleIndexCount);
			s_Data.Stats.DrawCalls++;
		}
		
		// --- Lines ---
		if (s_Data.LineVertexCount && s_Data.LineShader) {
			uint32_t size = (uint8_t*)s_Data.LineVertexBufferPtr - (uint8_t*)s_Data.LineVertexBufferBase;
			s_Data.LineVertexBuffer->SetData(s_Data.LineVertexBufferBase, size);
			
			s_Data.LineShader->Bind();
			s_Data.LineShader->SetMat4("u_ViewProjection", s_Data.CameraBuffer.ViewProjection);
			RenderCommand::SetLineWidth(s_Data.LineWidth);
			
			s_Data.LineVertexArray->Bind();
			RenderCommand::DrawLines(s_Data.LineVertexArray, s_Data.LineVertexCount);
			s_Data.Stats.DrawCalls++;
		}
	}
	
	// -------------------------------------------------------------------------
	// Stats
	// -------------------------------------------------------------------------
	void RendererPipeline2D::ResetStats() {
		memset(&s_Data.Stats, 0, sizeof(RendererPipeline2D::Statistics));
	}
	
	RendererPipeline2D::Statistics RendererPipeline2D::GetStats() {
		return s_Data.Stats;
	}
}