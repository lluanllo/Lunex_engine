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
#include "RHI/RHITexture.h"
#include "RHI/RHIFramebuffer.h"
#include "RHI/RHISampler.h"

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
		void RenderSceneDepthOnly(Scene* scene, const glm::mat4& lightVP);
		void RenderSceneDepthPoint(Scene* scene, const glm::vec3& lightPos, float lightRange,
								   const glm::mat4 faceVPs[6]);

	private:
		ShadowConfig m_Config;
		bool m_Enabled = true;
		bool m_Initialized = false;

		// Shadow atlas (RHI-abstracted)
		Ref<RHI::RHITexture2DArray> m_AtlasDepthTexture;
		Ref<RHI::RHIFramebuffer> m_AtlasFBO;
		Ref<RHI::RHISampler> m_ShadowSampler;
		uint32_t m_AtlasMaxLayers = 0;
		uint32_t m_AtlasResolution = 0;
		std::vector<bool> m_LayerOccupancy;

		// Shadow casters for current frame
		std::vector<ShadowCasterInfo> m_ShadowCasters;

		// GPU uniform data for depth shaders (binding = 6)
		struct DepthShaderUBOData {
			glm::mat4 LightVP;                    // 64 bytes
			glm::mat4 Model;                       // 64 bytes
			glm::vec4 LightPosAndRange;            // 16 bytes (xyz = pos, w = range)
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
