#pragma once

/**
 * @file ShadowSystem.h
 * @brief Main shadow system orchestrator
 *
 * Manages shadow map allocation, rendering, and GPU data upload.
 * Call Update() once per frame BEFORE Renderer3D::BeginScene().
 */

#include "ShadowTypes.h"
#include "CascadedShadowMap.h"
#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"
#include "RHI/RHI.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	// Forward declarations
	class Scene;
	class EditorCamera;
	class Camera;
	struct LightEntry;

	class ShadowSystem {
	public:
		static ShadowSystem& Get();

		void Initialize(const ShadowConfig& config = {});
		void Shutdown();

		// ----- Main entry points (call once per frame) -----

		void Update(Scene* scene, const EditorCamera& camera);
		void Update(Scene* scene, const Camera& camera, const glm::mat4& cameraTransform);

		// ----- Bind for scene rendering -----

		/** Bind shadow atlas texture and UBO for the main PBR pass */
		void BindForSceneRendering();

		// ----- Configuration -----

		void SetConfig(const ShadowConfig& config);
		const ShadowConfig& GetConfig() const { return m_Config; }

		void SetEnabled(bool enabled) { m_Enabled = enabled; }
		bool IsEnabled() const { return m_Enabled; }

		// ----- Statistics -----

		struct Statistics {
			uint32_t ShadowMapsRendered = 0;
			uint32_t ShadowDrawCalls = 0;
			uint32_t ShadowCulled = 0;
			uint32_t CascadesRendered = 0;
			uint32_t PointFacesRendered = 0;
			uint32_t SpotMapsRendered = 0;
		};
		const Statistics& GetStatistics() const { return m_Stats; }

	private:
		ShadowSystem() = default;
		~ShadowSystem() = default;
		ShadowSystem(const ShadowSystem&) = delete;
		ShadowSystem& operator=(const ShadowSystem&) = delete;

		// ----- Internal pipeline -----

		void UpdateInternal(
			Scene* scene,
			const glm::mat4& cameraView,
			const glm::mat4& cameraProj,
			const glm::vec3& cameraPos,
			float cameraNear,
			float cameraFar
		);

		void CollectShadowCasters(const glm::vec3& cameraPos);
		void AllocateAtlasLayers();
		void ComputeLightMatrices(
			const glm::mat4& cameraView,
			const glm::mat4& cameraProj,
			float cameraNear, float cameraFar
		);
		void RenderShadowMaps(Scene* scene);
		void UploadGPUData();

		// Render helpers
		// Draws every mesh whose world AABB intersects the light frustum.
		// cullNearPlane=false keeps casters that sit between the light and the shadow volume (CSM/spot).
		void RenderShadowCasters(Scene* scene, const glm::mat4& lightVP, bool cullNearPlane);

	private:
		ShadowConfig m_Config;
		bool m_Enabled = true;
		bool m_Initialized = false;

		// Shadow atlas (RHI resources)
		Ref<RHI::RHITexture2DArray> m_AtlasDepthTexture;   // Depth texture array
		Ref<RHI::RHIFramebuffer>    m_AtlasFBO;             // Depth-only FBO
		Ref<RHI::RHISampler> m_ShadowSampler;
		uint32_t m_AtlasMaxLayers = 0;
		uint32_t m_AtlasCapacityLimit = 0;   // hard upper bound (all cascades + 16 cube maps)
		void EnsureAtlasCapacity(uint32_t layers);

		// Far-cascade caching (FarCascadeUpdateInterval)
		uint64_t m_FrameIndex = 0;
		glm::mat4 m_CachedCascadeVP[MAX_SHADOW_CASCADES];
		float m_CachedCascadeTexel[MAX_SHADOW_CASCADES] = {};
		bool m_CascadeCacheValid[MAX_SHADOW_CASCADES] = {};
		glm::vec3 m_CachedSunDirection{ 0.0f };
		uint32_t m_AtlasResolution = 0;      // Uniform resolution for all layers
		std::vector<bool> m_LayerOccupancy;

		// Shadow casters for current frame
		std::vector<ShadowCasterInfo> m_ShadowCasters;

		// GPU uniform data for depth shaders (binding = 6)
		struct DepthShaderUBOData {
			glm::mat4 LightVP;                    // 64 bytes
			glm::mat4 Model;                       // 64 bytes
			glm::vec4 LightPosAndRange;            // 16 bytes (xyz = pos, w = range)
			int Skinned = 0;                       // 1 = apply bone palette (SSBO binding 4)
			int _pad[3] = { 0, 0, 0 };
		};

		// GPU data
		ShadowUniformData m_GPUData;
		Ref<UniformBuffer> m_ShadowUBO;       // binding = 7
		Ref<UniformBuffer> m_DepthShaderUBO;  // binding = 6
		DepthShaderUBOData m_DepthUBOData;

		// Shaders
		Ref<Shader> m_DepthShader;            // Directional + Spot
		Ref<Shader> m_DepthPointShader;       // Point lights (linear depth)

		// Stats
		Statistics m_Stats;
	};

} // namespace Lunex
