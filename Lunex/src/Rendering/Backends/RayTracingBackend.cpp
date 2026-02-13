#include "stpch.h"
#include "RayTracingBackend.h"
#include "Rendering/RenderSystem.h"
#include "Log/Log.h"

#include <glad/glad.h>

namespace Lunex {

	// ============================================================================
	// LIFECYCLE
	// ============================================================================

	void RayTracingBackend::Initialize(const RenderSystemConfig& config) {
		if (m_Initialized) return;

		LNX_LOG_INFO("[RayTracingBackend] Initializing...");

		m_ViewportWidth  = config.ViewportWidth;
		m_ViewportHeight = config.ViewportHeight;

		// Load compute shader
		m_PathTraceShader = Shader::CreateCompute("assets/shaders/PathTracing.comp.glsl");
		if (!m_PathTraceShader || !m_PathTraceShader->IsComputeShader()) {
			LNX_LOG_ERROR("[RayTracingBackend] Failed to load PathTracing compute shader!");
			return;
		}

		// Load composite (fullscreen blit) shader
		m_CompositeShader = Shader::Create("assets/shaders/RTComposite.glsl");
		if (!m_CompositeShader) {
			LNX_LOG_ERROR("[RayTracingBackend] Failed to load RTComposite shader!");
			return;
		}

		CreateOutputTexture(m_ViewportWidth, m_ViewportHeight);
		CreateAccumulationTexture(m_ViewportWidth, m_ViewportHeight);

		m_Initialized = true;
		m_SceneDirty = true;
		m_FrameIndex = 0;

		LNX_LOG_INFO("[RayTracingBackend] Initialized ({0}x{1})", m_ViewportWidth, m_ViewportHeight);
	}

	void RayTracingBackend::Shutdown() {
		if (!m_Initialized) return;

		LNX_LOG_INFO("[RayTracingBackend] Shutting down...");

		m_PathTraceShader.reset();
		m_CompositeShader.reset();
		m_OutputTexture.reset();
		m_AccumulationTexture.reset();

		m_Initialized = false;
	}

	void RayTracingBackend::OnViewportResize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) return;
		if (width == m_ViewportWidth && height == m_ViewportHeight) return;

		m_ViewportWidth  = width;
		m_ViewportHeight = height;

		CreateOutputTexture(width, height);
		CreateAccumulationTexture(width, height);

		m_FrameIndex = 0; // Reset accumulation on resize
	}

	// ============================================================================
	// TEXTURE CREATION
	// ============================================================================

	void RayTracingBackend::CreateOutputTexture(uint32_t w, uint32_t h) {
		RHI::TextureDesc desc(w, h, RHI::TextureFormat::RGBA32F);
		desc.IsStorage = true;
		desc.IsRenderTarget = true;
		desc.DebugName = "RT_Output";
		m_OutputTexture = RHI::RHITexture2D::Create(desc);
	}

	void RayTracingBackend::CreateAccumulationTexture(uint32_t w, uint32_t h) {
		RHI::TextureDesc desc(w, h, RHI::TextureFormat::RGBA32F);
		desc.IsStorage = true;
		desc.DebugName = "RT_Accumulation";
		m_AccumulationTexture = RHI::RHITexture2D::Create(desc);
	}

	// ============================================================================
	// PER-FRAME
	// ============================================================================

	void RayTracingBackend::PrepareSceneData(const SceneRenderData& sceneData) {
		if (!m_Initialized) return;

		// Rebuild BVH when scene geometry changes
		if (m_SceneDirty || !m_SceneData.IsBuilt()) {
			m_SceneData.Build(sceneData, sceneData.SourceScene);
			m_SceneDirty = false;
			m_FrameIndex = 0;
		}

		// Update per-frame camera / settings
		m_SceneData.UpdatePerFrame(
			sceneData.Camera,
			m_ViewportWidth, m_ViewportHeight,
			m_FrameIndex,
			m_Config.MaxBounces,
			m_Config.AccumulateFrames
		);
	}

	// ============================================================================
	// RENDER GRAPH
	// ============================================================================

	void RayTracingBackend::BuildRenderGraph(RenderGraph& graph, const SceneRenderInfo& sceneInfo) {
		if (!m_Initialized || !m_PathTraceShader || !m_CompositeShader) return;

		// -- Pass 1: Path Trace (compute) --
		graph.AddComputePass(
			"PathTrace",
			[&](RenderPassBuilder& builder) {
				builder.SetName("PathTrace");
				// The compute shader writes to m_OutputTexture via image2D,
				// no render graph resources needed — we manage them manually.
			},
			[this](const RenderPassResources& /*resources*/) {
				if (!m_SceneData.IsBuilt()) return;

				// Bind SSBOs
				if (m_SceneData.GetBVHBuffer())
					m_SceneData.GetBVHBuffer()->BindForCompute(0);
				if (m_SceneData.GetTriangleBuffer())
					m_SceneData.GetTriangleBuffer()->BindForCompute(1);
				if (m_SceneData.GetMaterialBuffer())
					m_SceneData.GetMaterialBuffer()->BindForCompute(2);
				if (m_SceneData.GetLightBuffer())
					m_SceneData.GetLightBuffer()->BindForCompute(3);

				// Bind scene info UBO
				if (m_SceneData.GetSceneInfoUBO())
					m_SceneData.GetSceneInfoUBO()->Bind(4);

				// Bind output image
				if (m_OutputTexture)
					m_OutputTexture->BindAsImage(0, RHI::BufferAccess::Write, 0);

				// Bind accumulation image
				if (m_AccumulationTexture)
					m_AccumulationTexture->BindAsImage(1, RHI::BufferAccess::ReadWrite, 0);

				// Dispatch compute shader
				m_PathTraceShader->Bind();

				uint32_t localSizeX = 16, localSizeY = 16;
				uint32_t groupsX = (m_ViewportWidth  + localSizeX - 1) / localSizeX;
				uint32_t groupsY = (m_ViewportHeight + localSizeY - 1) / localSizeY;

				m_PathTraceShader->DispatchCompute(groupsX, groupsY, 1);

				// Memory barrier — ensure image writes are visible
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				m_FrameIndex++;
			}
		);

		// -- Pass 2: Composite (fullscreen blit) --
		// Import the output texture into the render graph
		m_FinalColorTarget = graph.ImportTexture("RT_FinalColor", m_OutputTexture);
		graph.SetBackbufferSource(m_FinalColorTarget);

		graph.AddPass(
			"RTComposite",
			[&](RenderPassBuilder& builder) {
				builder.SetName("RTComposite");
				builder.ReadTexture(m_FinalColorTarget);
			},
			[this](const RenderPassResources& /*resources*/) {
				// Draw a fullscreen quad with the accumulation result
				glDisable(GL_DEPTH_TEST);

				m_CompositeShader->Bind();
				m_CompositeShader->SetFloat("u_Exposure", m_Config.Exposure);
				m_CompositeShader->SetInt("u_AccumulatedFrames", (int)m_FrameIndex);

				// Bind accumulation texture to sampler unit 0
				if (m_AccumulationTexture)
					m_AccumulationTexture->Bind(0);
				m_CompositeShader->SetInt("u_Texture", 0);

				// Render fullscreen triangle
				glBindVertexArray(0);
				glDrawArrays(GL_TRIANGLES, 0, 3);

				glEnable(GL_DEPTH_TEST);
			}
		);
	}

	// ============================================================================
	// CAPABILITIES
	// ============================================================================

	bool RayTracingBackend::SupportsFeature(const std::string& feature) const {
		if (feature == "PathTracing")   return true;
		if (feature == "ComputeShader") return true;
		if (feature == "BVH")           return true;
		return false;
	}

} // namespace Lunex
