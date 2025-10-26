#include "stpch.h"
#include "Renderer2D.h"
#include "Renderer/Renderer2D/RendererPipeline2D.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {
	Ref<RendererPipeline2D> Renderer2D::s_Pipeline;
	
	void Renderer2D::Init() {
		LNX_PROFILE_FUNCTION();
		s_Pipeline = CreateRef<RendererPipeline2D>();
		s_Pipeline->Init();
	}
	
	void Renderer2D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		s_Pipeline->Shutdown();
		s_Pipeline.reset();
	}
	
	void Renderer2D::BeginScene(const OrthographicCamera& camera) {
		LNX_PROFILE_FUNCTION();
		s_Pipeline->BeginScene(camera.GetViewProjectionMatrix());
	}
	
	void Renderer2D::BeginScene(const glm::mat4& viewProjection) {
		LNX_PROFILE_FUNCTION();
		s_Pipeline->BeginScene(viewProjection);
	}
	
	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& viewMatrix) {
		LNX_PROFILE_FUNCTION();
		glm::mat4 viewProjection = camera.GetProjection() * glm::inverse(viewMatrix);
		s_Pipeline->BeginScene(viewProjection);
	}
	
	void Renderer2D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();
		s_Pipeline->BeginScene(camera.GetViewProjection());
	}
	
	void Renderer2D::EndScene() {
		LNX_PROFILE_FUNCTION();
		s_Pipeline->EndScene();
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
		s_Pipeline->SubmitQuad(transform, color);
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		s_Pipeline->SubmitQuad(transform, color, entityID);
	}
	
	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		s_Pipeline->SubmitSprite(transform, texture, tilingFactor, tintColor, entityID);
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
		s_Pipeline->SubmitQuad(transform, color);
	}
	
	void Renderer2D::DrawRotatedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		s_Pipeline->SubmitRotatedQuad(transform, texture, tilingFactor, tintColor, entityID);
	}
	
	// -------------------------------------------------------------------------
	// CIRCLES
	// -------------------------------------------------------------------------
	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID) {
		s_Pipeline->SubmitCircle(transform, color, thickness, fade, entityID);
	}
	
	// -------------------------------------------------------------------------
	// LINES
	// -------------------------------------------------------------------------
	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {
		s_Pipeline->SubmitLine(p0, p1, color, entityID);
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
		s_Pipeline->ResetStats();
	}
	
	RendererPipeline2D::Statistics Renderer2D::GetStats() {
		return s_Pipeline->GetStats();
	}
}