#include "stpch.h"
#include "PostProcessRenderer.h"

#include "RHI/RHI.h"
#include "Renderer/UniformBuffer.h"
#include "Log/Log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace Lunex {

	// ============================================================================
	// INTERNAL DATA
	// ============================================================================

	struct BloomMip {
		glm::ivec2 Size = { 0, 0 };
		uint32_t TexID = 0;   // GL texture
	};

	struct PostProcessData {
		bool Initialized = false;

		PostProcessConfig Config;

		// ========== SHADERS ==========
		Ref<Shader> BloomDownsampleShader;
		Ref<Shader> BloomUpsampleShader;
		Ref<Shader> CompositeShader;

		// ========== FULL-SCREEN QUAD ==========
		Ref<VertexArray>  QuadVAO;
		Ref<VertexBuffer> QuadVBO;

		// ========== BLOOM RESOURCES ==========
		std::vector<BloomMip> BloomMips;
		uint32_t BloomFBO = 0;

		uint32_t CurrentWidth  = 0;
		uint32_t CurrentHeight = 0;
	};

	static PostProcessData s_Data;

	// ============================================================================
	// HELPERS
	// ============================================================================

	static void DrawFullScreenQuad() {
		s_Data.QuadVAO->Bind();
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->DrawArrays(6);
		}
	}

	// ============================================================================
	// INIT / SHUTDOWN
	// ============================================================================

	void PostProcessRenderer::Init() {
		LNX_PROFILE_FUNCTION();

		// Load shaders
		s_Data.BloomDownsampleShader = Shader::Create("assets/shaders/PostProcess_BloomDown.glsl");
		s_Data.BloomUpsampleShader   = Shader::Create("assets/shaders/PostProcess_BloomUp.glsl");
		s_Data.CompositeShader       = Shader::Create("assets/shaders/PostProcess_Composite.glsl");

		if (!s_Data.BloomDownsampleShader || !s_Data.BloomDownsampleShader->IsValid())
			LNX_LOG_ERROR("PostProcessRenderer: Failed to compile PostProcess_BloomDown shader!");
		if (!s_Data.BloomUpsampleShader || !s_Data.BloomUpsampleShader->IsValid())
			LNX_LOG_ERROR("PostProcessRenderer: Failed to compile PostProcess_BloomUp shader!");
		if (!s_Data.CompositeShader || !s_Data.CompositeShader->IsValid())
			LNX_LOG_ERROR("PostProcessRenderer: Failed to compile PostProcess_Composite shader!");

		// Create full-screen quad (same as DeferredRenderer)
		float quadVertices[] = {
			// positions        // texcoords
			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,

			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
		};

		s_Data.QuadVAO = VertexArray::Create();
		s_Data.QuadVBO = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		s_Data.QuadVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoords" }
		});
		s_Data.QuadVAO->AddVertexBuffer(s_Data.QuadVBO);

		// Create bloom FBO (we attach different textures per pass)
		glCreateFramebuffers(1, &s_Data.BloomFBO);

		s_Data.Initialized = true;
		LNX_LOG_INFO("PostProcessRenderer initialized");
	}

	void PostProcessRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();

		// Destroy bloom mip textures
		for (auto& mip : s_Data.BloomMips) {
			if (mip.TexID) {
				glDeleteTextures(1, &mip.TexID);
				mip.TexID = 0;
			}
		}
		s_Data.BloomMips.clear();

		if (s_Data.BloomFBO) {
			glDeleteFramebuffers(1, &s_Data.BloomFBO);
			s_Data.BloomFBO = 0;
		}

		s_Data.Initialized = false;
		LNX_LOG_INFO("PostProcessRenderer shutdown");
	}

	bool PostProcessRenderer::IsInitialized() {
		return s_Data.Initialized;
	}

	PostProcessConfig& PostProcessRenderer::GetConfig() {
		return s_Data.Config;
	}

	// ============================================================================
	// BLOOM RESOURCE CREATION
	// ============================================================================

	void PostProcessRenderer::CreateBloomResources(uint32_t width, uint32_t height) {
		// Destroy old mips
		for (auto& mip : s_Data.BloomMips) {
			if (mip.TexID) glDeleteTextures(1, &mip.TexID);
		}
		s_Data.BloomMips.clear();

		int levels = std::clamp(s_Data.Config.BloomMipLevels, 1, 8);
		glm::ivec2 mipSize = { width / 2, height / 2 };

		for (int i = 0; i < levels; i++) {
			BloomMip mip;
			mip.Size = { std::max(mipSize.x, 1), std::max(mipSize.y, 1) };

			glCreateTextures(GL_TEXTURE_2D, 1, &mip.TexID);
			glTextureStorage2D(mip.TexID, 1, GL_R11F_G11F_B10F, mip.Size.x, mip.Size.y);
			glTextureParameteri(mip.TexID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(mip.TexID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(mip.TexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(mip.TexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			s_Data.BloomMips.push_back(mip);
			mipSize /= 2;
		}

		s_Data.CurrentWidth  = width;
		s_Data.CurrentHeight = height;
	}

	// ============================================================================
	// VIEWPORT RESIZE
	// ============================================================================

	void PostProcessRenderer::OnViewportResize(uint32_t width, uint32_t height) {
		if (!s_Data.Initialized) return;
		if (width == s_Data.CurrentWidth && height == s_Data.CurrentHeight) return;
		CreateBloomResources(width, height);
	}

	// ============================================================================
	// BLOOM PASS
	// ============================================================================

	void PostProcessRenderer::ExecuteBloom(uint32_t sceneColorTexID, uint32_t width, uint32_t height) {
		if (!s_Data.BloomDownsampleShader || !s_Data.BloomDownsampleShader->IsValid()) return;
		if (!s_Data.BloomUpsampleShader   || !s_Data.BloomUpsampleShader->IsValid()) return;
		if (s_Data.BloomMips.empty()) return;

		glBindFramebuffer(GL_FRAMEBUFFER, s_Data.BloomFBO);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		// ===== DOWNSAMPLE PASS (with threshold on first iteration) =====
		s_Data.BloomDownsampleShader->Bind();
		s_Data.BloomDownsampleShader->SetFloat("u_Threshold", s_Data.Config.BloomThreshold);

		uint32_t srcTexture = sceneColorTexID;

		for (size_t i = 0; i < s_Data.BloomMips.size(); i++) {
			const auto& mip = s_Data.BloomMips[i];

			glNamedFramebufferTexture(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0, mip.TexID, 0);
			glViewport(0, 0, mip.Size.x, mip.Size.y);

			s_Data.BloomDownsampleShader->SetInt("u_SrcTexture", 0);
			s_Data.BloomDownsampleShader->SetFloat2("u_SrcResolution",
				i == 0 ? glm::vec2((float)width, (float)height)
					   : glm::vec2((float)s_Data.BloomMips[i - 1].Size.x,
								   (float)s_Data.BloomMips[i - 1].Size.y));
			// Only apply threshold on the first downsample
			s_Data.BloomDownsampleShader->SetInt("u_ApplyThreshold", i == 0 ? 1 : 0);

			glBindTextureUnit(0, srcTexture);
			DrawFullScreenQuad();

			srcTexture = mip.TexID;
		}

		// ===== UPSAMPLE PASS (additive) =====
		s_Data.BloomUpsampleShader->Bind();
		s_Data.BloomUpsampleShader->SetFloat("u_FilterRadius", s_Data.Config.BloomRadius);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);  // Additive blending

		for (int i = (int)s_Data.BloomMips.size() - 1; i > 0; i--) {
			const auto& srcMip = s_Data.BloomMips[i];
			const auto& dstMip = s_Data.BloomMips[i - 1];

			glNamedFramebufferTexture(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0, dstMip.TexID, 0);
			glViewport(0, 0, dstMip.Size.x, dstMip.Size.y);

			s_Data.BloomUpsampleShader->SetInt("u_SrcTexture", 0);
			glBindTextureUnit(0, srcMip.TexID);

			DrawFullScreenQuad();
		}

		glDisable(GL_BLEND);
	}

	// ============================================================================
	// EXECUTE ALL POST-PROCESSING
	// ============================================================================

	void PostProcessRenderer::Execute(uint32_t sceneColorTexID, const Ref<Framebuffer>& targetFramebuffer,
									  uint32_t width, uint32_t height) {
		LNX_PROFILE_FUNCTION();

		if (!s_Data.Initialized) return;

		bool anyEffectEnabled = s_Data.Config.EnableBloom ||
								s_Data.Config.EnableVignette ||
								s_Data.Config.EnableChromaticAberration;

		if (!anyEffectEnabled) return;

		// Ensure bloom resources match viewport size
		if (s_Data.Config.EnableBloom) {
			if (width != s_Data.CurrentWidth || height != s_Data.CurrentHeight ||
				s_Data.BloomMips.empty()) {
				CreateBloomResources(width, height);
			}
			ExecuteBloom(sceneColorTexID, width, height);
		}

		// ===== COMPOSITE PASS =====
		if (!s_Data.CompositeShader || !s_Data.CompositeShader->IsValid()) return;

		targetFramebuffer->Bind();

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetDrawBuffers({ 0 });
			cmdList->SetDepthTestEnabled(false);
			cmdList->SetDepthMask(false);
		}
		glDisable(GL_BLEND);

		s_Data.CompositeShader->Bind();

		// Bind scene color
		s_Data.CompositeShader->SetInt("u_SceneColor", 0);
		glBindTextureUnit(0, sceneColorTexID);

		// Bind bloom texture (first mip = result)
		bool hasBloom = s_Data.Config.EnableBloom && !s_Data.BloomMips.empty();
		s_Data.CompositeShader->SetInt("u_BloomTexture", 1);
		if (hasBloom) {
			glBindTextureUnit(1, s_Data.BloomMips[0].TexID);
		}

		// Set uniforms
		s_Data.CompositeShader->SetInt("u_EnableBloom", hasBloom ? 1 : 0);
		s_Data.CompositeShader->SetFloat("u_BloomIntensity", s_Data.Config.BloomIntensity);

		s_Data.CompositeShader->SetInt("u_EnableVignette", s_Data.Config.EnableVignette ? 1 : 0);
		s_Data.CompositeShader->SetFloat("u_VignetteIntensity", s_Data.Config.VignetteIntensity);
		s_Data.CompositeShader->SetFloat("u_VignetteRoundness", s_Data.Config.VignetteRoundness);
		s_Data.CompositeShader->SetFloat("u_VignetteSmoothness", s_Data.Config.VignetteSmoothness);

		s_Data.CompositeShader->SetInt("u_EnableChromaticAberration", s_Data.Config.EnableChromaticAberration ? 1 : 0);
		s_Data.CompositeShader->SetFloat("u_ChromaticAberrationIntensity", s_Data.Config.ChromaticAberrationIntensity);

		s_Data.CompositeShader->SetInt("u_ToneMapOperator", s_Data.Config.ToneMapOperator);
		s_Data.CompositeShader->SetFloat("u_Exposure", s_Data.Config.Exposure);
		s_Data.CompositeShader->SetFloat("u_Gamma", s_Data.Config.Gamma);

		DrawFullScreenQuad();

		// Restore state
		if (cmdList) {
			cmdList->SetDepthTestEnabled(true);
			cmdList->SetDepthMask(true);
			cmdList->SetDrawBuffers({ 0, 1 });
		}
	}

} // namespace Lunex
