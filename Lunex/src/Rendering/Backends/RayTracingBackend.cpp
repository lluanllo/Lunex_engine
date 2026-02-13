#include "stpch.h"
#include "RayTracingBackend.h"

#include "Renderer/SkyboxRenderer.h"
#include "Scene/Lighting/LightSystem.h"
#include "Log/Log.h"

#include <glad/glad.h>
#include <functional>

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

	// Minimum SSBO allocation to avoid tiny buffers
	static constexpr uint32_t MIN_LIGHTS_SSBO_BYTES = 1024;

	// ====================================================================
	// LIFECYCLE
	// ====================================================================

	void RayTracingBackend::Initialize() {
		m_RTScene.Initialize();

		m_CameraUBO  = UniformBuffer::Create(sizeof(CameraUBOData), BIND_CAMERA_UBO);

		// Initial lights SSBO — will grow on demand
		m_LightsSSBOCapacity = MIN_LIGHTS_SSBO_BYTES;
		m_LightsSSBO = StorageBuffer::Create(m_LightsSSBOCapacity, BIND_LIGHTS);

		LNX_LOG_INFO("RayTracingBackend: Initialized");
	}

	void RayTracingBackend::Shutdown() {
		if (m_AccumulationTexID) { glDeleteTextures(1, &m_AccumulationTexID); m_AccumulationTexID = 0; }
		if (m_OutputTexID)       { glDeleteTextures(1, &m_OutputTexID);       m_OutputTexID = 0; }
		m_RTScene.Shutdown();
		m_CameraUBO.reset();
		m_LightsSSBO.reset();
		m_LightsSSBOCapacity = 0;
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

		// Detect camera movement ? reset accumulation
		glm::mat4 curVP = m_SceneData.ViewProjection;
		if (curVP != m_LastVP) {
			ResetAccumulation();
			m_LastVP = curVP;
		}

		// Detect scene geometry/material/light changes ? rebuild BVH + reset
		if (DetectSceneChanges()) {
			m_RTScene.MarkDirty();
			ResetAccumulation();
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

		if (DetectSceneChanges()) {
			m_RTScene.MarkDirty();
			ResetAccumulation();
		}
	}

	// ====================================================================
	// SCENE CHANGE DETECTION
	// ====================================================================

	size_t RayTracingBackend::ComputeSceneHash() const {
		size_t hash = 0;

		// Hash draw items: transform + mesh pointer + material pointer + entity ID
		for (const auto& item : m_SceneData.DrawItems) {
			// Hash transform matrix (first row is enough for change detection)
			const float* m = &item.Transform[0][0];
			for (int i = 0; i < 16; ++i) {
				hash ^= std::hash<float>{}(m[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			}
			hash ^= std::hash<void*>{}(item.MeshModel.get()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<void*>{}(item.Material.get())   + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}

		// Hash lights
		for (const auto& light : m_SceneData.Lights) {
			const float* v = &light.Position.x;
			for (int i = 0; i < 20; ++i) {  // 5 vec4 = 20 floats
				hash ^= std::hash<float>{}(v[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			}
		}

		// Hash counts
		hash ^= std::hash<size_t>{}(m_SceneData.DrawItems.size()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<size_t>{}(m_SceneData.Lights.size())    + 0x9e3779b9 + (hash << 6) + (hash >> 2);

		return hash;
	}

	bool RayTracingBackend::DetectSceneChanges() {
		size_t currentHash = ComputeSceneHash();
		if (currentHash != m_LastSceneHash) {
			m_LastSceneHash = currentHash;
			return true;
		}
		return false;
	}

	// ====================================================================
	// LIGHTS UPLOAD (grow-on-demand)
	// ====================================================================

	void RayTracingBackend::UploadLights() {
		uint32_t lightCount = static_cast<uint32_t>(m_SceneData.Lights.size());
		uint32_t requiredBytes = lightCount * static_cast<uint32_t>(sizeof(LightData));

		if (requiredBytes == 0) return;

		// Grow SSBO if needed (2x strategy)
		if (requiredBytes > m_LightsSSBOCapacity) {
			uint32_t newCap = std::max(requiredBytes * 2, MIN_LIGHTS_SSBO_BYTES);
			m_LightsSSBO = StorageBuffer::Create(newCap, BIND_LIGHTS);
			m_LightsSSBOCapacity = newCap;
		}

		m_LightsSSBO->SetData(m_SceneData.Lights.data(), requiredBytes);
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

		// Upload lights (grow-on-demand, no per-frame allocation)
		UploadLights();

		// Bind resources that stay constant across all samples this frame
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

		uint32_t groupsX = (m_Width  + 7) / 8;
		uint32_t groupsY = (m_Height + 7) / 8;

		// Dispatch N samples per frame (SamplesPerFrame setting)
		uint32_t samplesThisFrame = std::max(1u, m_Settings.SamplesPerFrame);
		for (uint32_t s = 0; s < samplesThisFrame; ++s) {
			// Clamp to max if set
			if (m_Settings.MaxAccumulatedSamples > 0 &&
			    m_SampleCount >= m_Settings.MaxAccumulatedSamples)
				break;

			// Fill camera UBO with current sample state
			m_CameraData.InverseProjection = m_SceneData.InverseProjection;
			m_CameraData.InverseView       = m_SceneData.InverseView;
			m_CameraData.CameraPosition    = glm::vec4(m_SceneData.CameraPosition, 1.0f);
			m_CameraData.FrameIndex        = m_FrameIndex;
			m_CameraData.SampleCount       = m_SampleCount;
			m_CameraData.MaxBounces        = m_Settings.MaxBounces;
			m_CameraData.SamplesPerFrame   = samplesThisFrame;
			m_CameraData.TriangleCount     = m_RTScene.GetTriangleCount();
			m_CameraData.BVHNodeCount      = m_RTScene.GetBVHNodeCount();
			m_CameraData.LightCount        = static_cast<uint32_t>(m_SceneData.Lights.size());
			m_CameraData.MaterialCount     = m_RTScene.GetMaterialCount();
			m_CameraData.RussianRoulette   = m_Settings.RussianRouletteThresh;
			m_CameraData._pad0 = 0.0f;
			m_CameraData._pad1 = 0.0f;
			m_CameraData._pad2 = 0.0f;
			m_CameraUBO->SetData(&m_CameraData, sizeof(CameraUBOData));

			m_PathTracerShader->Dispatch(groupsX, groupsY, 1);

			m_SampleCount++;
			m_FrameIndex++;
		}

		// Final barrier so the output texture is ready for display
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
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
		m_LastSceneHash = 0;
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
