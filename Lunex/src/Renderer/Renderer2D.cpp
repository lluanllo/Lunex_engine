#include "stpch.h"
#include "Renderer2D.h"

#include "VertexArray.h"
#include "Shader.h"
#include "UniformBuffer.h"
#include "RHI/RHI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
		
		// Editor-only
		int EntityID;
	};
	
	struct LineVertex {
		glm::vec3 Position;
		glm::vec4 Color;
		
		// Editor-only
		int EntityID;
	};
	
	struct Renderer2DData {
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO: RenderCaps
		
		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> QuadShader;
		Ref<Texture2D> WhiteTexture;
		
		Ref<VertexArray> CircleVertexArray;
		Ref<VertexBuffer> CircleVertexBuffer;
		Ref<Shader> CircleShader;
		
		Ref<VertexArray> LineVertexArray;
		Ref<VertexBuffer> LineVertexBuffer;
		Ref<Shader> LineShader;
		
		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;
		
		uint32_t CircleIndexCount = 0;
		CircleVertex* CircleVertexBufferBase = nullptr;
		CircleVertex* CircleVertexBufferPtr = nullptr;
		
		uint32_t LineVertexCount = 0;
		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;
		
		float LineWidth = 2.0f;
		
		std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = white texture
		
		glm::vec4 QuadVertexPositions[4];
		
		Renderer2D::Statistics Stats;
		
		struct CameraData {
			glm::mat4 ViewProjection;
		};
		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;
	};
	
	static Renderer2DData s_Data;
	
	void Renderer2D::Init() {
		LNX_PROFILE_FUNCTION();
		
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
		
		s_Data.QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];
		
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
		
		// Lines
		s_Data.LineVertexArray = VertexArray::Create();
		
		s_Data.LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
		s_Data.LineVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Int,    "a_EntityID" }
		});
		s_Data.LineVertexArray->AddVertexBuffer(s_Data.LineVertexBuffer);
		s_Data.LineVertexBufferBase = new LineVertex[s_Data.MaxVertices];
		
		// Circles
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
		s_Data.CircleVertexArray->SetIndexBuffer(quadIB); // Use quad IB
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
		
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));
		
		int32_t samplers[s_Data.MaxTextureSlots];
		for (uint32_t i = 0; i < s_Data.MaxTextureSlots; i++)
			samplers[i] = i;
		
		s_Data.QuadShader = Shader::Create("assets/shaders/Renderer2D_Quad.glsl");
		s_Data.CircleShader = Shader::Create("assets/shaders/Renderer2D_Circle.glsl");
		s_Data.LineShader = Shader::Create("assets/shaders/Renderer2D_Line.glsl");
		
		// Set all texture slots to 0
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;
		
		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };
		
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);
	}
	
	void Renderer2D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		delete[] s_Data.QuadVertexBufferBase;
	}
	
	void Renderer2D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		StartBatch();
	}
	
	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform) {
		
		LNX_PROFILE_FUNCTION();
		
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		
		StartBatch();
	}
	
	void Renderer2D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();
		
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		
		StartBatch();
	}
	
	void Renderer2D::EndScene() {
		LNX_PROFILE_FUNCTION();
		
		Flush();
	}
	
	void Renderer2D::StartBatch() {
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
		
		s_Data.CircleIndexCount = 0;
		s_Data.CircleVertexBufferPtr = s_Data.CircleVertexBufferBase;
		
		s_Data.LineVertexCount = 0;
		s_Data.LineVertexBufferPtr = s_Data.LineVertexBufferBase;
		
		s_Data.TextureSlotIndex = 1; // keep white texture at slot 0, do NOT clear other slots here (Hazel style)
		// (Previously cleared texture slots each batch causing unnecessary rebinding and potential ordering artifacts)
	}
	
	void Renderer2D::Flush() {
		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) return;
		
		if (s_Data.QuadIndexCount) {
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);
			// Bind textures
			for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
				s_Data.TextureSlots[i]->Bind(i);
			
			s_Data.QuadShader->Bind();
			s_Data.QuadVertexArray->Bind();
			cmdList->DrawIndexed(s_Data.QuadIndexCount);
			s_Data.Stats.DrawCalls++;
		}
		
		if (s_Data.CircleIndexCount) {
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase);
			s_Data.CircleVertexBuffer->SetData(s_Data.CircleVertexBufferBase, dataSize);
			
			s_Data.CircleShader->Bind();
			s_Data.CircleVertexArray->Bind();
			cmdList->DrawIndexed(s_Data.CircleIndexCount);
			s_Data.Stats.DrawCalls++;
		}
		
		if (s_Data.LineVertexCount) {
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.LineVertexBufferPtr - (uint8_t*)s_Data.LineVertexBufferBase);
			s_Data.LineVertexBuffer->SetData(s_Data.LineVertexBufferBase, dataSize);
			
			s_Data.LineShader->Bind();
			s_Data.LineVertexArray->Bind();
			cmdList->SetLineWidth(s_Data.LineWidth);
			cmdList->DrawLines(s_Data.LineVertexCount);
			s_Data.Stats.DrawCalls++;
		}
	}
	
	void Renderer2D::NextBatch() {
		Flush();
		StartBatch();
	}
	
	void Renderer2D::DrawLine(const glm::vec3& p0, glm::vec3& p1, const glm::vec4& color, int entityID) {
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
	
	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID) {
		glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
		glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);
		
		DrawLine(p0, p1, color, entityID);
		DrawLine(p1, p2, color, entityID);
		DrawLine(p2, p3, color, entityID);
		DrawLine(p3, p0, color, entityID);
	}
	
	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		glm::vec3 lineVertices[4];
		for (size_t i = 0; i < 4; i++)
			lineVertices[i] = transform * s_Data.QuadVertexPositions[i];
		
		DrawLine(lineVertices[0], lineVertices[1], color, entityID);
		DrawLine(lineVertices[1], lineVertices[2], color, entityID);
		DrawLine(lineVertices[2], lineVertices[3], color, entityID);
		DrawLine(lineVertices[3], lineVertices[0], color, entityID);
	}
	
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}
	
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		DrawQuad(transform, color);
	}
	
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor) {
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}
	
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		DrawQuad(transform, texture, tilingFactor);
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;
		
		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();
		
		for (size_t i = 0; i < quadVertexCount; i++) {
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}
		
		s_Data.QuadIndexCount += 6;
		
		s_Data.Stats.QuadCount++;
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		
		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();
		
		// Validate texture
		if (!texture || !texture->IsLoaded()) {
			// If texture is invalid, draw as solid color quad instead
			DrawQuad(transform, tintColor, entityID);
			return;
		}
		
		float textureIndex = 0.0f;
		
		// Search for texture in already bound slots
		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++) {
			if (s_Data.TextureSlots[i] && s_Data.TextureSlots[i]->GetRendererID() == texture->GetRendererID()) {
				textureIndex = (float)i;
				break;
			}
		}
		
		// If texture not found, add it to a new slot
		if (textureIndex == 0.0f) {
			if (s_Data.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				NextBatch();
			
			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}
		
		for (size_t i = 0; i < quadVertexCount; i++) {
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}
		
		s_Data.QuadIndexCount += 6;
		
		s_Data.Stats.QuadCount++;
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		DrawQuad(transform, color);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor) {
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		DrawQuad(transform, texture, tilingFactor, tintColor);
	}
	
	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness /*= 1.0f*/, float fade /*= 0.005f*/, int entityID /*= -1*/) {
		LNX_PROFILE_FUNCTION();
		
		// TODO: implement for circles
		// if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
		// 	NextBatch();
		
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
	
	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID) {
		if (src.Texture)
			DrawQuad(transform, src.Texture, src.TilingFactor, src.Color, entityID);
		else
			DrawQuad(transform, src.Color, entityID);
	}

	void Renderer2D::DrawBillboard(const glm::vec3& position, const Ref<Texture2D>& texture, 
									const glm::vec3& cameraPosition, float size, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		// Calculate direction from position to camera
		glm::vec3 toCamera = glm::normalize(cameraPosition - position);
		
		// Calculate right and up vectors for billboard
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 right = glm::normalize(glm::cross(worldUp, toCamera));
		glm::vec3 up = glm::cross(toCamera, right);
		
		// Build billboard transform matrix
		glm::mat4 transform = glm::mat4(1.0f);
		transform[0] = glm::vec4(right * size, 0.0f);
		transform[1] = glm::vec4(up * size, 0.0f);
		transform[2] = glm::vec4(toCamera, 0.0f);
		transform[3] = glm::vec4(position, 1.0f);
		
		// Draw as textured quad with neutral tint (no color modulation)
		DrawQuad(transform, texture, 1.0f, glm::vec4(1.0f), entityID);
	}
	
	void Renderer2D::DrawCameraFrustum(const glm::mat4& projection, const glm::mat4& view, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		// ========================================
		// LIMIT FAR PLANE DISTANCE (avoid infinite extension)
		// ========================================
		
		// Extract camera position from view matrix
		glm::mat4 invView = glm::inverse(view);
		glm::vec3 cameraPos = glm::vec3(invView[3]);
		
		// Calculate forward direction
		glm::vec3 forward = -glm::vec3(invView[2]); // Camera looks down -Z
		
		// Limit far plane distance (like in the reference image)
		const float maxFrustumDepth = 1.0f; // ✅ Adjustable frustum depth
		
		// Calculate inverse view-projection matrix to transform NDC corners to world space
		glm::mat4 invViewProj = glm::inverse(projection * view);
		
		// Define frustum corners in NDC space (Normalized Device Coordinates)
		// Near plane corners (z = -1 in NDC)
		glm::vec4 nearCorners[4] = {
			{ -1.0f, -1.0f, -1.0f, 1.0f },  // Bottom-left
			{  1.0f, -1.0f, -1.0f, 1.0f },  // Bottom-right
			{  1.0f,  1.0f, -1.0f, 1.0f },  // Top-right
			{ -1.0f,  1.0f, -1.0f, 1.0f }   // Top-left
		};
		
		// Far plane corners (z = 1 in NDC) - but we'll clamp these
		glm::vec4 farCorners[4] = {
			{ -1.0f, -1.0f, 1.0f, 1.0f },   // Bottom-left
			{  1.0f, -1.0f, 1.0f, 1.0f },   // Bottom-right
			{  1.0f,  1.0f, 1.0f, 1.0f },   // Top-right
			{ -1.0f,  1.0f, 1.0f, 1.0f }    // Top-left
		};
		
		// Transform corners to world space
		glm::vec3 nearWorld[4];
		glm::vec3 farWorld[4];
		
		for (int i = 0; i < 4; i++) {
			// Transform near corners
			glm::vec4 worldNear = invViewProj * nearCorners[i];
			worldNear /= worldNear.w;  // Perspective divide
			nearWorld[i] = glm::vec3(worldNear);
			
			// Transform far corners
			glm::vec4 worldFar = invViewProj * farCorners[i];
			worldFar /= worldFar.w;  // Perspective divide
			
			// ✅ CLAMP FAR PLANE: Limit distance from camera
			glm::vec3 farPos = glm::vec3(worldFar);
			glm::vec3 direction = glm::normalize(farPos - cameraPos);
			float distance = glm::length(farPos - cameraPos);
			
			if (distance > maxFrustumDepth) {
				farPos = cameraPos + direction * maxFrustumDepth;
			}
			
			farWorld[i] = farPos;
		}
		
		// ✅ USE BLACK COLOR with thin lines
		glm::vec4 blackColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		
		// Draw near plane rectangle
		DrawLine(nearWorld[0], nearWorld[1], blackColor, entityID);  // Bottom
		DrawLine(nearWorld[1], nearWorld[2], blackColor, entityID);  // Right
		DrawLine(nearWorld[2], nearWorld[3], blackColor, entityID);  // Top
		DrawLine(nearWorld[3], nearWorld[0], blackColor, entityID);  // Left
		
		// Draw far plane rectangle
		DrawLine(farWorld[0], farWorld[1], blackColor, entityID);  // Bottom
		DrawLine(farWorld[1], farWorld[2], blackColor, entityID);  // Right
		DrawLine(farWorld[2], farWorld[3], blackColor, entityID);  // Top
		DrawLine(farWorld[3], farWorld[0], blackColor, entityID);  // Left
		
		// Draw connecting lines between near and far planes
		DrawLine(nearWorld[0], farWorld[0], blackColor, entityID);  // Bottom-left
		DrawLine(nearWorld[1], farWorld[1], blackColor, entityID);  // Bottom-right
		DrawLine(nearWorld[2], farWorld[2], blackColor, entityID);  // Top-right
		DrawLine(nearWorld[3], farWorld[3], blackColor, entityID);  // Top-left
	}
	
	// ========================================
	// LIGHT GIZMOS IMPLEMENTATION
	// ========================================
	
	void Renderer2D::DrawPointLightGizmo(const glm::vec3& position, float radius, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		// Draw 3 circles (XY, XZ, YZ planes) to represent sphere
		const int segments = 32;
		const float angleStep = glm::two_pi<float>() / segments;
		
		// XY plane circle (around Z axis)
		for (int i = 0; i < segments; i++) {
			float angle1 = i * angleStep;
			float angle2 = (i + 1) * angleStep;
			
			glm::vec3 p1 = position + glm::vec3(
				glm::cos(angle1) * radius,
				glm::sin(angle1) * radius,
				0.0f
			);
			glm::vec3 p2 = position + glm::vec3(
				glm::cos(angle2) * radius,
				glm::sin(angle2) * radius,
				0.0f
			);
			
			DrawLine(p1, p2, color, entityID);
		}
		
		// XZ plane circle (around Y axis)
		for (int i = 0; i < segments; i++) {
			float angle1 = i * angleStep;
			float angle2 = (i + 1) * angleStep;
			
			glm::vec3 p1 = position + glm::vec3(
				glm::cos(angle1) * radius,
				0.0f,
				glm::sin(angle1) * radius
			);
			glm::vec3 p2 = position + glm::vec3(
				glm::cos(angle2) * radius,
				0.0f,
				glm::sin(angle2) * radius
			);
			
			DrawLine(p1, p2, color, entityID);
		}
		
		// YZ plane circle (around X axis)
		for (int i = 0; i < segments; i++) {
			float angle1 = i * angleStep;
			float angle2 = (i + 1) * angleStep;
			
			glm::vec3 p1 = position + glm::vec3(
				0.0f,
				glm::cos(angle1) * radius,
				glm::sin(angle1) * radius
			);
			glm::vec3 p2 = position + glm::vec3(
				0.0f,
				glm::cos(angle2) * radius,
				glm::sin(angle2) * radius
			);
			
			DrawLine(p1, p2, color, entityID);
		}
	}
	
	void Renderer2D::DrawDirectionalLightGizmo(const glm::vec3& position, const glm::vec3& direction, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		// Draw arrow showing direction
		const float arrowLength = 2.0f;
		const float arrowHeadSize = 0.3f;
		
		glm::vec3 dir = glm::normalize(direction);
		glm::vec3 endPoint = position + dir * arrowLength;
		
		// Main arrow line
		DrawLine(position, endPoint, color, entityID);
		
		// Arrow head (cone-like)
		// Calculate perpendicular vectors for arrow head
		glm::vec3 up = glm::abs(dir.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 right = glm::normalize(glm::cross(up, dir));
		up = glm::normalize(glm::cross(dir, right));
		
		// Arrow head base position
		glm::vec3 headBase = endPoint - dir * arrowHeadSize;
		
		// Draw 4 lines from tip to base in a cone shape
		const int headSegments = 8;
		for (int i = 0; i < headSegments; i++) {
			float angle = (i / (float)headSegments) * glm::two_pi<float>();
			float x = glm::cos(angle) * arrowHeadSize * 0.5f;
			float y = glm::sin(angle) * arrowHeadSize * 0.5f;
			
			glm::vec3 headPoint = headBase + right * x + up * y;
			DrawLine(endPoint, headPoint, color, entityID);
			
			// Connect to next point to form cone base
			float nextAngle = ((i + 1) / (float)headSegments) * glm::two_pi<float>();
			float nextX = glm::cos(nextAngle) * arrowHeadSize * 0.5f;
			float nextY = glm::sin(nextAngle) * arrowHeadSize * 0.5f;
			glm::vec3 nextHeadPoint = headBase + right * nextX + up * nextY;
			
			DrawLine(headPoint, nextHeadPoint, color, entityID);
		}
	}
	
	void Renderer2D::DrawSpotLightGizmo(const glm::vec3& position, const glm::vec3& direction, float range, float outerConeAngle, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();
		
		glm::vec3 dir = glm::normalize(direction);
		
		// Calculate cone dimensions
		float coneRadius = glm::tan(glm::radians(outerConeAngle)) * range;
		glm::vec3 coneEnd = position + dir * range;
		
		// Calculate perpendicular vectors for cone base
		glm::vec3 up = glm::abs(dir.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 right = glm::normalize(glm::cross(up, dir));
		up = glm::normalize(glm::cross(dir, right));
		
		// Draw cone outline
		const int segments = 16;
		const float angleStep = glm::two_pi<float>() / segments;
		
		// Draw base circle
		for (int i = 0; i < segments; i++) {
			float angle1 = i * angleStep;
			float angle2 = (i + 1) * angleStep;
			
			glm::vec3 p1 = coneEnd + right * (glm::cos(angle1) * coneRadius) + up * (glm::sin(angle1) * coneRadius);
			glm::vec3 p2 = coneEnd + right * (glm::cos(angle2) * coneRadius) + up * (glm::sin(angle2) * coneRadius);
			
			DrawLine(p1, p2, color, entityID);
		}
		
		// Draw lines from apex to base (8 lines for clarity)
		for (int i = 0; i < 8; i++) {
			float angle = (i / 8.0f) * glm::two_pi<float>();
			glm::vec3 basePoint = coneEnd + right * (glm::cos(angle) * coneRadius) + up * (glm::sin(angle) * coneRadius);
			DrawLine(position, basePoint, color, entityID);
		}
	}
	
	// ========================================
	// 3D WIREFRAME SHAPES (Collider Visualization)
	// ========================================

	void Renderer2D::DrawWireBox(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		LNX_PROFILE_FUNCTION();

		// 8 corners of a unit cube [-0.5, 0.5]
		glm::vec3 corners[8];
		int idx = 0;
		for (int z = 0; z <= 1; z++) {
			for (int y = 0; y <= 1; y++) {
				for (int x = 0; x <= 1; x++) {
					glm::vec4 local(x - 0.5f, y - 0.5f, z - 0.5f, 1.0f);
					corners[idx++] = glm::vec3(transform * local);
				}
			}
		}

		// 12 edges of the cube
		// Bottom face (z=0): 0-1, 1-3, 3-2, 2-0
		DrawLine(corners[0], corners[1], color, entityID);
		DrawLine(corners[1], corners[3], color, entityID);
		DrawLine(corners[3], corners[2], color, entityID);
		DrawLine(corners[2], corners[0], color, entityID);

		// Top face (z=1): 4-5, 5-7, 7-6, 6-4
		DrawLine(corners[4], corners[5], color, entityID);
		DrawLine(corners[5], corners[7], color, entityID);
		DrawLine(corners[7], corners[6], color, entityID);
		DrawLine(corners[6], corners[4], color, entityID);

		// Vertical edges: 0-4, 1-5, 2-6, 3-7
		DrawLine(corners[0], corners[4], color, entityID);
		DrawLine(corners[1], corners[5], color, entityID);
		DrawLine(corners[2], corners[6], color, entityID);
		DrawLine(corners[3], corners[7], color, entityID);
	}

	void Renderer2D::DrawWireSphere(const glm::mat4& transform, const glm::vec4& color, int segments, int entityID) {
		LNX_PROFILE_FUNCTION();

		const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

		// XY plane circle
		for (int i = 0; i < segments; i++) {
			float a1 = i * angleStep;
			float a2 = (i + 1) * angleStep;
			glm::vec3 p1 = glm::vec3(transform * glm::vec4(glm::cos(a1) * 0.5f, glm::sin(a1) * 0.5f, 0.0f, 1.0f));
			glm::vec3 p2 = glm::vec3(transform * glm::vec4(glm::cos(a2) * 0.5f, glm::sin(a2) * 0.5f, 0.0f, 1.0f));
			DrawLine(p1, p2, color, entityID);
		}

		// XZ plane circle
		for (int i = 0; i < segments; i++) {
			float a1 = i * angleStep;
			float a2 = (i + 1) * angleStep;
			glm::vec3 p1 = glm::vec3(transform * glm::vec4(glm::cos(a1) * 0.5f, 0.0f, glm::sin(a1) * 0.5f, 1.0f));
			glm::vec3 p2 = glm::vec3(transform * glm::vec4(glm::cos(a2) * 0.5f, 0.0f, glm::sin(a2) * 0.5f, 1.0f));
			DrawLine(p1, p2, color, entityID);
		}

		// YZ plane circle
		for (int i = 0; i < segments; i++) {
			float a1 = i * angleStep;
			float a2 = (i + 1) * angleStep;
			glm::vec3 p1 = glm::vec3(transform * glm::vec4(0.0f, glm::cos(a1) * 0.5f, glm::sin(a1) * 0.5f, 1.0f));
			glm::vec3 p2 = glm::vec3(transform * glm::vec4(0.0f, glm::cos(a2) * 0.5f, glm::sin(a2) * 0.5f, 1.0f));
			DrawLine(p1, p2, color, entityID);
		}
	}

	void Renderer2D::DrawWireCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color, int segments, int entityID) {
		LNX_PROFILE_FUNCTION();

		// Capsule = cylinder body + two hemisphere caps
		// Height is total height (including caps), body half-height:
		float halfBody = (height * 0.5f) - radius;
		if (halfBody < 0.0f) halfBody = 0.0f;

		const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

		// Top and bottom ring of the cylinder body
		for (int i = 0; i < segments; i++) {
			float a1 = i * angleStep;
			float a2 = (i + 1) * angleStep;

			// Top ring
			glm::vec3 t1 = glm::vec3(transform * glm::vec4(glm::cos(a1) * radius, halfBody, glm::sin(a1) * radius, 1.0f));
			glm::vec3 t2 = glm::vec3(transform * glm::vec4(glm::cos(a2) * radius, halfBody, glm::sin(a2) * radius, 1.0f));
			DrawLine(t1, t2, color, entityID);

			// Bottom ring
			glm::vec3 b1 = glm::vec3(transform * glm::vec4(glm::cos(a1) * radius, -halfBody, glm::sin(a1) * radius, 1.0f));
			glm::vec3 b2 = glm::vec3(transform * glm::vec4(glm::cos(a2) * radius, -halfBody, glm::sin(a2) * radius, 1.0f));
			DrawLine(b1, b2, color, entityID);
		}

		// 4 vertical lines connecting rings
		for (int i = 0; i < 4; i++) {
			float a = i * glm::half_pi<float>();
			glm::vec3 top = glm::vec3(transform * glm::vec4(glm::cos(a) * radius, halfBody, glm::sin(a) * radius, 1.0f));
			glm::vec3 bot = glm::vec3(transform * glm::vec4(glm::cos(a) * radius, -halfBody, glm::sin(a) * radius, 1.0f));
			DrawLine(top, bot, color, entityID);
		}

		// Top hemisphere (2 half-circles: XY and ZY)
		int halfSegments = segments / 2;
		float halfAngleStep = glm::pi<float>() / static_cast<float>(halfSegments);
		for (int i = 0; i < halfSegments; i++) {
			float a1 = i * halfAngleStep;
			float a2 = (i + 1) * halfAngleStep;

			// XY arc (top half)
			glm::vec3 p1 = glm::vec3(transform * glm::vec4(glm::sin(a1) * radius, halfBody + glm::cos(a1) * radius, 0.0f, 1.0f));
			glm::vec3 p2 = glm::vec3(transform * glm::vec4(glm::sin(a2) * radius, halfBody + glm::cos(a2) * radius, 0.0f, 1.0f));
			DrawLine(p1, p2, color, entityID);

			// ZY arc (top half)
			p1 = glm::vec3(transform * glm::vec4(0.0f, halfBody + glm::cos(a1) * radius, glm::sin(a1) * radius, 1.0f));
			p2 = glm::vec3(transform * glm::vec4(0.0f, halfBody + glm::cos(a2) * radius, glm::sin(a2) * radius, 1.0f));
			DrawLine(p1, p2, color, entityID);
		}

		// Bottom hemisphere (2 half-circles: XY and ZY)
		for (int i = 0; i < halfSegments; i++) {
			float a1 = glm::pi<float>() + i * halfAngleStep;
			float a2 = glm::pi<float>() + (i + 1) * halfAngleStep;

			// XY arc (bottom half)
			glm::vec3 p1 = glm::vec3(transform * glm::vec4(glm::sin(a1) * radius, -halfBody + glm::cos(a1) * radius, 0.0f, 1.0f));
			glm::vec3 p2 = glm::vec3(transform * glm::vec4(glm::sin(a2) * radius, -halfBody + glm::cos(a2) * radius, 0.0f, 1.0f));
			DrawLine(p1, p2, color, entityID);

			// ZY arc (bottom half)
			p1 = glm::vec3(transform * glm::vec4(0.0f, -halfBody + glm::cos(a1) * radius, glm::sin(a1) * radius, 1.0f));
			p2 = glm::vec3(transform * glm::vec4(0.0f, -halfBody + glm::cos(a2) * radius, glm::sin(a2) * radius, 1.0f));
			DrawLine(p1, p2, color, entityID);
		}
	}

	float Renderer2D::GetLineWidth() {
		return s_Data.LineWidth;
	}
	
	void Renderer2D::SetLineWidth(float width) {
		s_Data.LineWidth = width;
	}
	
	void Renderer2D::ResetStats() {
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}
	
	Renderer2D::Statistics Renderer2D::GetStats() {
		return s_Data.Stats;
	}
}