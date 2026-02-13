#include "stpch.h"
#include "RayTracingBackend.h"

#include "Renderer/SkyboxRenderer.h"
#include "Scene/Lighting/LightSystem.h"
#include "Log/Log.h"

#include <glad/glad.h>

namespace Lunex {

	// SSBO / UBO binding points used by the path tracer shader
	static constexpr uint32_t BIND_TRIANGLES  = 20;
	static constexpr uint32_t BIND_BVH        = 21;
	static constexpr uint32_t BIND_MATERIALS   = 22;
	static constexpr uint32_t BIND_LIGHTS      = 23;
	static constexpr uint32_t BIND_CAMERA_UBO  = 15;

	// Image unit bindings
	static constexpr uint32_t IMG_ACCUMULATION = 0;
	static constexpr uint32_t IMG_OUTPUT       = 1;

	// ====================================================================
	// LIFECYCLE
	// ====================================================================

	void RayTracingBackend::Initialize() {
		m_RTScene.Initialize();

		m_CameraUBO  = UniformBuffer::Create(sizeof(CameraUBOData), BIND_CAMERA_UBO);
		m_LightsSSBO = StorageBuffer::Create(sizeof(LightData) * 128, BIND_LIGHTS);

		// Compute shader will be loaded when the viewport size is known
		// (we need it for the output textures anyway)
		LNX_LOG_INFO("RayTracingBackend: Initialized");
	}

	void RayTracingBackend::Shutdown() {
		if (m_AccumulationTexID) { glDeleteTextures(1, &m_AccumulationTexID); m_AccumulationTexID = 0; }
		if (m_OutputTexID)       { glDeleteTextures(1, &m_OutputTexID);       m_OutputTexID = 0; }
		m_RTScene.Shutdown();
		m_CameraUBO.reset();
		m_LightsSSBO.reset();
		m_PathTracerShader.reset();
		LNX_LOG_INFO("RayTracingBackend: Shutdown");
	}

	// ====================================================================
	// OUTPUT TEXTURES
	// ====================================================================

	void RayTracingBackend::CreateOutputTextures(uint32_t w, uint32_t h) {
		if (m_AccumulationTexID) glDeleteTextures(1, &m_AccumulationTexID);
		if (m_OutputTexID)       glDeleteTextures(1, &m_OutputTexID);

		// Accumulation: RGBA32F
		glGenTextures(1, &m_AccumulationTexID);
		glBindTexture(GL_TEXTURE_2D, m_AccumulationTexID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Output: RGBA8
		glGenTextures(1, &m_OutputTexID);
		glBindTexture(GL_TEXTURE_2D, m_OutputTexID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);

		m_Width  = w;
		m_Height = h;
	}

	uint32_t RayTracingBackend::GetOutputTextureID() const {
		return m_OutputTexID;
	}

	void RayTracingBackend::OnViewportResize(uint32_t w, uint32_t h) {
		if (w == 0 || h == 0) return;
		if (w == m_Width && h == m_Height) return;

		CreateOutputTextures(w, h);
		ResetAccumulation();

		// Load compute shader on first resize (guarantees a valid viewport)
		if (!m_PathTracerShader) {
			m_PathTracerShader = RHI::RHIShader::CreateComputeFromFile(
				"assets/shaders/PathTracer.glsl");
			if (!m_PathTracerShader || !m_PathTracerShader->IsValid()) {
				LNX_LOG_ERROR("RayTracingBackend: Failed to load PathTracer.glsl");
				m_PathTracerShader.reset();
			}
		}
	}

	// ====================================================================
	// BEGIN FRAME
	// ====================================================================

	void RayTracingBackend::BeginFrame(const EditorCamera& camera) {
		SceneDataCollector::Collect(m_CurrentScene, camera, m_SceneData);
		m_SceneData.ViewportWidth  = m_Width;
		m_SceneData.ViewportHeight = m_Height;

		// Detect camera movement -> reset accumulation
		glm::mat4 curVP = m_SceneData.ViewProjection;
		if (curVP != m_LastVP) {
			ResetAccumulation();
			m_LastVP = curVP;
		}
	}

	void RayTracingBackend::BeginFrameRuntime(const Camera& camera,
	                                           const glm::mat4& cameraTransform) {
		SceneDataCollector::Collect(m_CurrentScene, camera, cameraTransform, m_SceneData);
		m_SceneData.ViewportWidth  = m_Width;
		m_SceneData.ViewportHeight = m_Height;

		glm::mat4 curVP = m_SceneData.ViewProjection;
		if (curVP != m_LastVP) {
			ResetAccumulation();
			m_LastVP = curVP;
		}
	}

	// ====================================================================
	// RENDER SCENE
	// ====================================================================

	void RayTracingBackend::RenderScene(Scene* scene) {
		if (!m_PathTracerShader || m_Width == 0 || m_Height == 0)
			return;

		// Respect max accumulated samples
		if (m_Settings.MaxAccumulatedSamples > 0 &&
		    m_SampleCount >= m_Settings.MaxAccumulatedSamples)
			return;

		// Rebuild BVH / materials if scene is dirty
		if (m_RTScene.IsDirty()) {
			m_RTScene.Rebuild(m_SceneData);
		}

		// Upload lights
		if (!m_SceneData.Lights.empty()) {
			uint32_t lightsSize = static_cast<uint32_t>(
				m_SceneData.Lights.size() * sizeof(LightData));
			if (lightsSize > 0) {
				// Grow if needed
				m_LightsSSBO = StorageBuffer::Create(
					std::max(lightsSize, 1024u), BIND_LIGHTS);
				m_LightsSSBO->SetData(m_SceneData.Lights.data(), lightsSize);
			}
		}

		// Fill camera UBO
		m_CameraData.InverseProjection = m_SceneData.InverseProjection;
		m_CameraData.InverseView       = m_SceneData.InverseView;
		m_CameraData.CameraPosition    = glm::vec4(m_SceneData.CameraPosition, 1.0f);
		m_CameraData.FrameIndex        = m_FrameIndex;
		m_CameraData.SampleCount       = m_SampleCount;
		m_CameraData.MaxBounces        = m_Settings.MaxBounces;
		m_CameraData.TriangleCount     = m_RTScene.GetTriangleCount();
		m_CameraData.BVHNodeCount      = m_RTScene.GetBVHNodeCount();
		m_CameraData.LightCount        = static_cast<uint32_t>(m_SceneData.Lights.size());
		m_CameraData.MaterialCount     = m_RTScene.GetMaterialCount();
		m_CameraData.RussianRoulette   = m_Settings.RussianRouletteThresh;
		m_CameraUBO->SetData(&m_CameraData, sizeof(CameraUBOData));

		// Bind resources
		m_PathTracerShader->Bind();

		m_RTScene.Bind(BIND_TRIANGLES, BIND_BVH, BIND_MATERIALS);
		m_LightsSSBO->BindForCompute(BIND_LIGHTS);

		// Bind output images
		glBindImageTexture(IMG_ACCUMULATION, m_AccumulationTexID, 0, GL_FALSE, 0,
		                   GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(IMG_OUTPUT, m_OutputTexID, 0, GL_FALSE, 0,
		                   GL_WRITE_ONLY, GL_RGBA8);

		// Bind IBL cubemaps (same slots as raster)
		auto globalEnv = SkyboxRenderer::GetGlobalEnvironment();
		if (globalEnv && globalEnv->IsLoaded()) {
			if (globalEnv->GetIrradianceMap())  globalEnv->GetIrradianceMap()->Bind(8);
			if (globalEnv->GetPrefilteredMap()) globalEnv->GetPrefilteredMap()->Bind(9);
			if (globalEnv->GetBRDFLUT())        globalEnv->GetBRDFLUT()->Bind(10);
		}

		// Dispatch
		uint32_t groupsX = (m_Width  + 7) / 8;
		uint32_t groupsY = (m_Height + 7) / 8;
		m_PathTracerShader->Dispatch(groupsX, groupsY, 1);

		// Memory barrier so the output texture is ready
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		m_SampleCount++;
		m_FrameIndex++;
	}

	void RayTracingBackend::EndFrame() {
		// Nothing — output texture is ready after Dispatch.
	}

	// ====================================================================
	// SCENE NOTIFICATIONS
	// ====================================================================

	void RayTracingBackend::OnSceneChanged(Scene* scene) {
		m_CurrentScene = scene;
		m_RTScene.MarkDirty();
		ResetAccumulation();
	}

	// ====================================================================
	// ACCUMULATION
	// ====================================================================

	void RayTracingBackend::ResetAccumulation() {
		m_SampleCount = 0;
		m_FrameIndex  = 0;

		// Clear the accumulation buffer
		if (m_AccumulationTexID && m_Width > 0 && m_Height > 0) {
			float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glClearTexImage(m_AccumulationTexID, 0, GL_RGBA, GL_FLOAT, clearColor);
		}
	}

	void RayTracingBackend::UpdateCameraUBO() {
		m_CameraUBO->SetData(&m_CameraData, sizeof(CameraUBOData));
	}

	// ====================================================================
	// STATS
	// ====================================================================

	RenderBackendStats RayTracingBackend::GetStats() const {
		RenderBackendStats s;
		s.AccumulatedSamples = m_SampleCount;
		s.TotalTriangles     = m_RTScene.GetTriangleCount();
		s.BVHNodeCount       = m_RTScene.GetBVHNodeCount();
		return s;
	}

} // namespace Lunex
