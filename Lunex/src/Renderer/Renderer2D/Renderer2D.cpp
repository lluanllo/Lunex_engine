#include "stpch.h"
#include "Renderer2D.h"
#include "Renderer/Renderer2D/RendererPipeline2D.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {
	void Renderer2D::Init() {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::Init();
	}
	
	void Renderer2D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::Shutdown();
	}
	
	void Renderer2D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::BeginScene(camera.GetViewProjectionMatrix());
	}
	
	void Renderer2D::BeginScene(const glm::mat4& viewProjection) {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::BeginScene(viewProjection);
	}
	
	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& viewMatrix) {
		LNX_PROFILE_FUNCTION();
		glm::mat4 viewProjection = camera.GetProjection() * glm::inverse(viewMatrix);
		RendererPipeline2D::BeginScene(viewProjection);
	}
	
	void Renderer2D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::BeginScene(camera.GetViewProjection());
	}
	
	void Renderer2D::EndScene() {
		LNX_PROFILE_FUNCTION();
		RendererPipeline2D::EndScene();
	}
	
	// -------------------------------------------------------------------------
	// QUADS
	// -------------------------------------------------------------------------
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}
	
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		RendererPipeline2D::SubmitQuad(transform, color);
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		RendererPipeline2D::SubmitQuad(transform, color, entityID);
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		RendererPipeline2D::SubmitSprite(transform, texture, tilingFactor, tintColor, entityID);
	}
	
	// -------------------------------------------------------------------------
	// ROTATED QUADS
	// -------------------------------------------------------------------------
	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		RendererPipeline2D::SubmitQuad(transform, color);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		RendererPipeline2D::SubmitRotatedQuad(transform, texture, tilingFactor, tintColor, entityID);
	}
	
	// -------------------------------------------------------------------------
	// CIRCLES
	// -------------------------------------------------------------------------
	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID) {
		RendererPipeline2D::SubmitCircle(transform, color, thickness, fade, entityID);
	}
	
	// -------------------------------------------------------------------------
	// LINES
	// -------------------------------------------------------------------------
	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {
		RendererPipeline2D::SubmitLine(p0, p1, color, entityID);
	}
	
	void Renderer2D::SetLineWidth(float width) {
		RenderCommand::SetLineWidth(width);
	}
	
	float Renderer2D::GetLineWidth() {
		return 2.0f; // default; puedes devolver el del pipeline si lo expones
	}
	
	// -------------------------------------------------------------------------
	// STATS
	// -------------------------------------------------------------------------
	void Renderer2D::ResetStats() {
		RendererPipeline2D::ResetStats();
	}
	
	RendererPipeline2D::Statistics Renderer2D::GetStats() {
		return RendererPipeline2D::GetStats();
	}
}