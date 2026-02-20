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

	// UBO data matching shader layout(std140, binding = 0) uniform BloomDownParams
	struct BloomDownParamsUBO {
		glm::vec2 SrcResolution;
		float Threshold;
		int ApplyThreshold;
	};

	// UBO data matching shader layout(std140, binding = 0) uniform BloomUpParams
	struct BloomUpParamsUBO {
		float FilterRadius;
		float _pad1;
		float _pad2;
		float _pad3;
	};

	// UBO data matching shader layout(std140, binding = 0) uniform CompositeParams
	struct CompositeParamsUBO {
		int   EnableBloom;
		float BloomIntensity;
		int   EnableVignette;
		float VignetteIntensity;
		float VignetteRoundness;
		float VignetteSmoothness;
		int   EnableChromaticAberration;
		float ChromaticAberrationIntensity;
		int   ToneMapOperator;
		float Exposure;
		float Gamma;
		float _pad0;
	};

	struct PostProcessData {
		bool Initialized = false;

		PostProcessConfig Config;

		// ========== SHADERS ==========
		Ref<Shader> BloomDownsampleShader;
		Ref<Shader> BloomUpsampleShader;
		Ref<Shader> CompositeShader;

		// ========== UNIFORM BUFFERS ==========
		Ref<UniformBuffer> BloomDownUBO;
		Ref<UniformBuffer> BloomUpUBO;
		Ref<UniformBuffer> CompositeUBO;

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

		// Create UBOs (binding 0 for each shader - they are separate programs)
		s_Data.BloomDownUBO = UniformBuffer::Create(sizeof(BloomDownParamsUBO), 0);
		s_Data.BloomUpUBO   = UniformBuffer::Create(sizeof(BloomUpParamsUBO), 0);
		s_Data.CompositeUBO = UniformBuffer::Create(sizeof(CompositeParamsUBO), 0);

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

		uint32_t srcTexture = sceneColorTexID;

		for (size_t i = 0; i < s_Data.BloomMips.size(); i++) {
			const auto& mip = s_Data.BloomMips[i];

			glNamedFramebufferTexture(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0, mip.TexID, 0);
			glViewport(0, 0, mip.Size.x, mip.Size.y);

			// Upload UBO data per iteration
			BloomDownParamsUBO downParams;
			downParams.SrcResolution = i == 0
				? glm::vec2((float)width, (float)height)
				: glm::vec2((float)s_Data.BloomMips[i - 1].Size.x,
							(float)s_Data.BloomMips[i - 1].Size.y);
			downParams.Threshold = s_Data.Config.BloomThreshold;
			downParams.ApplyThreshold = (i == 0) ? 1 : 0;
			s_Data.BloomDownUBO->SetData(&downParams, sizeof(BloomDownParamsUBO));

			glBindTextureUnit(0, srcTexture);
			DrawFullScreenQuad();

			srcTexture = mip.TexID;
		}

		// ===== UPSAMPLE PASS (additive) =====
		s_Data.BloomUpsampleShader->Bind();

		BloomUpParamsUBO upParams;
		upParams.FilterRadius = s_Data.Config.BloomRadius;
		upParams._pad1 = 0.0f;
		upParams._pad2 = 0.0f;
		upParams._pad3 = 0.0f;
		s_Data.BloomUpUBO->SetData(&upParams, sizeof(BloomUpParamsUBO));

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);  // Additive blending

		for (int i = (int)s_Data.BloomMips.size() - 1; i > 0; i--) {
			const auto& srcMip = s_Data.BloomMips[i];
			const auto& dstMip = s_Data.BloomMips[i - 1];

			glNamedFramebufferTexture(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0, dstMip.TexID, 0);
			glViewport(0, 0, dstMip.Size.x, dstMip.Size.y);

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

		// Restore viewport to full resolution after bloom mip passes
		glViewport(0, 0, width, height);

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetDrawBuffers({ 0 });
			cmdList->SetDepthTestEnabled(false);
			cmdList->SetDepthMask(false);
		}
		glDisable(GL_BLEND);

		s_Data.CompositeShader->Bind();

		// Bind textures
		glBindTextureUnit(0, sceneColorTexID);

		bool hasBloom = s_Data.Config.EnableBloom && !s_Data.BloomMips.empty();
		if (hasBloom) {
			glBindTextureUnit(1, s_Data.BloomMips[0].TexID);
		}

		// Upload composite UBO
		CompositeParamsUBO compositeParams;
		compositeParams.EnableBloom = hasBloom ? 1 : 0;
		compositeParams.BloomIntensity = s_Data.Config.BloomIntensity;
		compositeParams.EnableVignette = s_Data.Config.EnableVignette ? 1 : 0;
		compositeParams.VignetteIntensity = s_Data.Config.VignetteIntensity;
		compositeParams.VignetteRoundness = s_Data.Config.VignetteRoundness;
		compositeParams.VignetteSmoothness = s_Data.Config.VignetteSmoothness;
		compositeParams.EnableChromaticAberration = s_Data.Config.EnableChromaticAberration ? 1 : 0;
		compositeParams.ChromaticAberrationIntensity = s_Data.Config.ChromaticAberrationIntensity;
		compositeParams.ToneMapOperator = s_Data.Config.ToneMapOperator;
		compositeParams.Exposure = s_Data.Config.Exposure;
		compositeParams.Gamma = s_Data.Config.Gamma;
		compositeParams._pad0 = 0.0f;
		s_Data.CompositeUBO->SetData(&compositeParams, sizeof(CompositeParamsUBO));

		DrawFullScreenQuad();

		// Restore state
		if (cmdList) {
			cmdList->SetDepthTestEnabled(true);
			cmdList->SetDepthMask(true);
			cmdList->SetDrawBuffers({ 0, 1 });
		}
	}

} // namespace Lunex
