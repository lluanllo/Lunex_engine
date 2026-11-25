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
		int   EnableSSR;
	};

	// UBO data matching shader layout(std140, binding = 0) uniform SSRParams
	struct SSRParamsUBO {
		glm::mat4 View;
		glm::mat4 Projection;
		float MaxDistance;
		float Thickness;
		int   MaxSteps;
		float MaxRoughness;
		float Intensity;
		float EdgeFade;
		float _pad0;
		float _pad1;
	};

	struct PostProcessData {
		bool Initialized = false;

		PostProcessConfig Config;

		// ========== SHADERS ==========
		Ref<Shader> BloomDownsampleShader;
		Ref<Shader> BloomUpsampleShader;
		Ref<Shader> CompositeShader;
		Ref<Shader> SSRShader;

		// ========== UNIFORM BUFFERS ==========
		Ref<UniformBuffer> BloomDownUBO;
		Ref<UniformBuffer> BloomUpUBO;
		Ref<UniformBuffer> CompositeUBO;
		Ref<UniformBuffer> SSRUBO;

		// ========== FRAME RESOURCES ==========
		// Copy of the scene color: effects read from here while writing the real target
		// (reading and writing the same texture in one pass is undefined behavior).
		uint32_t SceneCopyTex = 0;
		uint32_t SceneCopyFormat = 0;
		uint32_t SSRTex = 0;
		uint32_t FrameWidth = 0;
		uint32_t FrameHeight = 0;
		PostProcessFrameInputs Inputs;

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
		s_Data.SSRShader             = Shader::Create("assets/shaders/PostProcess_SSR.glsl");
		if (!s_Data.SSRShader || !s_Data.SSRShader->IsValid())
			LNX_LOG_ERROR("PostProcessRenderer: Failed to compile PostProcess_SSR shader!");

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
		s_Data.SSRUBO       = UniformBuffer::Create(sizeof(SSRParamsUBO), 0);

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

		if (s_Data.SceneCopyTex) { glDeleteTextures(1, &s_Data.SceneCopyTex); s_Data.SceneCopyTex = 0; }
		if (s_Data.SSRTex)       { glDeleteTextures(1, &s_Data.SSRTex);       s_Data.SSRTex = 0; }

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
	// FRAME RESOURCES (scene copy + SSR target)
	// ============================================================================

	void PostProcessRenderer::CreateFrameResources(uint32_t width, uint32_t height, uint32_t sceneFormat) {
		if (s_Data.SceneCopyTex) glDeleteTextures(1, &s_Data.SceneCopyTex);
		if (s_Data.SSRTex)       glDeleteTextures(1, &s_Data.SSRTex);

		auto makeTex = [](uint32_t& id, GLenum format, uint32_t w, uint32_t h) {
			glCreateTextures(GL_TEXTURE_2D, 1, &id);
			glTextureStorage2D(id, 1, format, w, h);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		};

		makeTex(s_Data.SceneCopyTex, sceneFormat, width, height);
		makeTex(s_Data.SSRTex, GL_RGBA16F, width, height);

		s_Data.SceneCopyFormat = sceneFormat;
		s_Data.FrameWidth = width;
		s_Data.FrameHeight = height;
	}

	void PostProcessRenderer::SetFrameInputs(const PostProcessFrameInputs& inputs) {
		s_Data.Inputs = inputs;
	}

	// ============================================================================
	// SSR PASS
	// ============================================================================

	void PostProcessRenderer::ExecuteSSR(uint32_t width, uint32_t height) {
		const auto& in = s_Data.Inputs;
		if (!s_Data.SSRShader || !s_Data.SSRShader->IsValid()) return;
		if (!in.NormalRoughnessTex || !in.PositionSpecularTex || !in.DepthTex || !in.AlbedoMetallicTex) return;

		glBindFramebuffer(GL_FRAMEBUFFER, s_Data.BloomFBO);
		glNamedFramebufferTexture(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0, s_Data.SSRTex, 0);
		glNamedFramebufferDrawBuffer(s_Data.BloomFBO, GL_COLOR_ATTACHMENT0);
		glViewport(0, 0, width, height);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		s_Data.SSRShader->Bind();

		SSRParamsUBO params;
		params.View = in.View;
		params.Projection = in.Projection;
		params.MaxDistance = std::max(s_Data.Config.SSRMaxDistance, 0.1f);
		params.Thickness = std::max(s_Data.Config.SSRThickness, 0.001f);
		params.MaxSteps = std::clamp(s_Data.Config.SSRMaxSteps, 8, 256);
		params.MaxRoughness = std::clamp(s_Data.Config.SSRMaxRoughness, 0.05f, 1.0f);
		params.Intensity = s_Data.Config.SSRIntensity;
		params.EdgeFade = 0.1f;
		params._pad0 = params._pad1 = 0.0f;
		s_Data.SSRUBO->SetData(&params, sizeof(SSRParamsUBO));

		glBindTextureUnit(0, s_Data.SceneCopyTex);
		glBindTextureUnit(1, in.NormalRoughnessTex);
		glBindTextureUnit(2, in.AlbedoMetallicTex);
		glBindTextureUnit(3, in.PositionSpecularTex);
		glBindTextureUnit(4, in.DepthTex);

		DrawFullScreenQuad();
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

		if (!s_Data.Initialized || width == 0 || height == 0) return;

		// ===== COPY SCENE COLOR =====
		// Everything below samples the copy; the composite then writes the real target.
		GLint sceneFormat = GL_RGBA16F;
		glGetTextureLevelParameteriv(sceneColorTexID, 0, GL_TEXTURE_INTERNAL_FORMAT, &sceneFormat);
		if (width != s_Data.FrameWidth || height != s_Data.FrameHeight ||
			static_cast<uint32_t>(sceneFormat) != s_Data.SceneCopyFormat || !s_Data.SceneCopyTex) {
			CreateFrameResources(width, height, static_cast<uint32_t>(sceneFormat));
		}
		glCopyImageSubData(sceneColorTexID, GL_TEXTURE_2D, 0, 0, 0, 0,
						   s_Data.SceneCopyTex, GL_TEXTURE_2D, 0, 0, 0, 0,
						   width, height, 1);
		const uint32_t sourceTex = s_Data.SceneCopyTex;

		// ===== SSR =====
		const bool hasSSR = s_Data.Config.EnableSSR;
		if (hasSSR) {
			ExecuteSSR(width, height);
		}

		// Ensure bloom resources match viewport size
		if (s_Data.Config.EnableBloom) {
			if (width != s_Data.CurrentWidth || height != s_Data.CurrentHeight ||
				s_Data.BloomMips.empty()) {
				CreateBloomResources(width, height);
			}
			ExecuteBloom(sourceTex, width, height);
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

		// Bind textures (the copy is CLAMP_TO_EDGE, avoiding edge artifacts with chromatic aberration)
		glBindTextureUnit(0, sourceTex);
		if (hasSSR) {
			glBindTextureUnit(2, s_Data.SSRTex);
		}

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
		compositeParams.EnableSSR = hasSSR ? 1 : 0;
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
