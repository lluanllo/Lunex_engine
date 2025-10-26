#include "stpch.h"
#include "RenderSystem2D.h"
#include "Renderer/Renderer2D/RendererPipeline2D.h"
#include "Renderer/RenderCore/RenderCommand.h"

namespace Lunex {
	Scope<RendererPipeline2D> RenderSystem2D::s_Pipeline = nullptr;
	
	void RenderSystem2D::Init() {
		LNX_PROFILE_FUNCTION();
		
		s_Pipeline = CreateScope<RendererPipeline2D>();
		s_Pipeline->Init();
	}
	
	void RenderSystem2D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		
		s_Pipeline->Shutdown();
		s_Pipeline.reset();
	}
	
	void RenderSystem2D::BeginScene(const glm::mat4& viewProjection) {
		LNX_PROFILE_FUNCTION();
		
		s_Pipeline->BeginScene(viewProjection);
	}
	
	void RenderSystem2D::EndScene() {
		LNX_PROFILE_FUNCTION();
		
		s_Pipeline->EndScene();
	}
	
	void RenderSystem2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID) {
		s_Pipeline->SubmitQuad(transform, color, entityID);
	}
	
	void RenderSystem2D::DrawSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID) {
		s_Pipeline->SubmitSprite(transform, texture, tilingFactor, tintColor, entityID);
	}
	
	void RenderSystem2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID) {
		s_Pipeline->SubmitCircle(transform, color, thickness, fade, entityID);
	}
	
	void RenderSystem2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {
		s_Pipeline->SubmitLine(p0, p1, color, entityID);
	}
	
	void RenderSystem2D::ResetStats() {
		s_Pipeline->ResetStats();
	}
	
	RendererPipeline2D::Statistics RenderSystem2D::GetStats() {
		return s_Pipeline->GetStats();
	}
}