#include "stpch.h"
#include "ShadowSystem.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/Lighting/LightSystem.h"
#include "Scene/Camera/EditorCamera.h"
#include "Resources/Mesh/Model.h"
#include "Log/Log.h"
#include "RHI/RHI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Lunex {

	ShadowSystem& ShadowSystem::Get() {
		static ShadowSystem instance;
		return instance;
	}

	// ========================================================================
	// INITIALIZATION / SHUTDOWN
	// ========================================================================

	void ShadowSystem::Initialize(const ShadowConfig& config) {
		if (m_Initialized) return;

		m_Config = config;

		// ---- Calculate total atlas layers needed (worst case) ----
		m_AtlasMaxLayers = MAX_SHADOW_CASCADES + MAX_SHADOW_LIGHTS * 6;
		m_LayerOccupancy.resize(m_AtlasMaxLayers, false);

		// ---- Create depth texture array (atlas) via RHI ----
		m_AtlasResolution = std::max({ m_Config.DirectionalResolution,
								  m_Config.SpotResolution,
								  m_Config.PointResolution });

		m_AtlasDepthTexture = RHI::RHITexture2DArray::Create(
			m_AtlasResolution, m_AtlasResolution, m_AtlasMaxLayers,
			RHI::TextureFormat::Depth32F, 1
		);

		// ---- Create shadow comparison sampler via RHI ----
		m_ShadowSampler = RHI::RHISampler::CreateShadow();

		// ---- Create depth-only FBO via RHI ----
		// We create a minimal FBO (no color attachments) with a depth attachment.
		// The depth layer will be swapped per-caster via AttachDepthTextureLayer.
		RHI::FramebufferDesc fboDesc;
		fboDesc.Width = m_AtlasResolution;
		fboDesc.Height = m_AtlasResolution;
		fboDesc.HasDepth = false; // We'll attach layers manually
		fboDesc.DebugName = "ShadowAtlasFBO";
		m_AtlasFBO = RHI::RHIFramebuffer::Create(fboDesc);

		// ---- Create UBO (binding = 7) ----
		m_ShadowUBO = UniformBuffer::Create(sizeof(ShadowUniformData), 7);
		memset(&m_GPUData, 0, sizeof(m_GPUData));

		// ---- Create depth shader UBO (binding = 6) ----
		m_DepthShaderUBO = UniformBuffer::Create(sizeof(DepthShaderUBOData), 6);
		memset(&m_DepthUBOData, 0, sizeof(m_DepthUBOData));

		// ---- Load shaders ----
		m_DepthShader = Shader::Create("assets/shaders/ShadowDepth.glsl");
		m_DepthPointShader = Shader::Create("assets/shaders/ShadowDepthPoint.glsl");

		m_Initialized = true;
		LNX_LOG_INFO("ShadowSystem initialized: atlas {0}x{0} x{1} layers, Depth32F",
					 m_AtlasResolution, m_AtlasMaxLayers);
	}

	void ShadowSystem::Shutdown() {
		if (!m_Initialized) return;

		m_AtlasFBO.reset();
		m_AtlasDepthTexture.reset();
		m_ShadowSampler.reset();
		m_DepthShader.reset();
		m_DepthPointShader.reset();
		m_ShadowUBO.reset();
		m_DepthShaderUBO.reset();
		m_ShadowCasters.clear();
		m_LayerOccupancy.clear();
		m_Initialized = false;

		LNX_LOG_INFO("ShadowSystem shut down");
	}

	// ========================================================================
	// PUBLIC UPDATE ENTRY POINTS
	// ========================================================================

	void ShadowSystem::Update(Scene* scene, const EditorCamera& camera) {
		if (!m_Initialized || !m_Enabled || !scene) return;

		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 proj = camera.GetProjection();
		glm::vec3 pos = camera.GetPosition();
		float nearClip = camera.GetNearClip();
		float farClip = camera.GetFarClip();

		UpdateInternal(scene, view, proj, pos, nearClip, farClip);
	}

	void ShadowSystem::Update(Scene* scene, const Camera& camera, const glm::mat4& cameraTransform) {
		if (!m_Initialized || !m_Enabled || !scene) return;

		glm::mat4 view = glm::inverse(cameraTransform);
		glm::mat4 proj = camera.GetProjection();
		glm::vec3 pos = glm::vec3(cameraTransform[3]);

		UpdateInternal(scene, view, proj, pos, 0.1f, 1000.0f);
	}

	// ========================================================================
	// BIND FOR SCENE RENDERING
	// ========================================================================

	void ShadowSystem::BindForSceneRendering() {
		if (!m_Initialized || !m_Enabled) return;

		// Bind shadow atlas to texture slot 11 via RHI
		m_AtlasDepthTexture->Bind(11);
		m_ShadowSampler->Bind(11);

		// Upload UBO
		m_ShadowUBO->SetData(&m_GPUData, sizeof(ShadowUniformData));
	}

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	void ShadowSystem::SetConfig(const ShadowConfig& config) {
		bool needsResize = (config.DirectionalResolution != m_Config.DirectionalResolution ||
							config.SpotResolution != m_Config.SpotResolution ||
							config.PointResolution != m_Config.PointResolution);
		m_Config = config;

		if (needsResize && m_Initialized) {
			Shutdown();
			Initialize(config);
		}
	}

	// ========================================================================
	// INTERNAL UPDATE PIPELINE
	// ========================================================================

	void ShadowSystem::UpdateInternal(
		Scene* scene,
		const glm::mat4& cameraView,
		const glm::mat4& cameraProj,
		const glm::vec3& cameraPos,
		float cameraNear,
		float cameraFar)
	{
		LNX_PROFILE_FUNCTION();

		m_Stats = {};

		// 1. Collect lights that cast shadows
		CollectShadowCasters(cameraPos);

		if (m_ShadowCasters.empty()) {
			// No shadow casters — zero out GPU data
			memset(&m_GPUData, 0, sizeof(m_GPUData));
			m_ShadowUBO->SetData(&m_GPUData, sizeof(ShadowUniformData));
			return;
		}

		// 2. Allocate atlas layers
		AllocateAtlasLayers();

		// 3. Compute light-space matrices (CSM splits, spot perspective, point cubemap)
		ComputeLightMatrices(cameraView, cameraProj, cameraNear, cameraFar);

		// 4. Render depth into shadow atlas
		RenderShadowMaps(scene);

		// 5. Upload GPU data
		UploadGPUData();
	}

	// ========================================================================
	// STEP 1: COLLECT SHADOW CASTERS
	// ========================================================================

	void ShadowSystem::CollectShadowCasters(const glm::vec3& cameraPos) {
		m_ShadowCasters.clear();
		std::fill(m_LayerOccupancy.begin(), m_LayerOccupancy.end(), false);

		const auto& lights = LightSystem::Get().GetAllLights();

		for (int i = 0; i < static_cast<int>(lights.size()); ++i) {
			const auto& light = lights[i];
			if (!light.IsActive || !light.Properties.CastShadows) continue;

			ShadowCasterInfo info;
			info.LightIndex = i;
			info.Position = light.WorldPosition;
			info.Direction = light.WorldDirection;
			info.Range = light.Properties.Range;
			info.InnerCone = light.Properties.InnerConeAngle;
			info.OuterCone = light.Properties.OuterConeAngle;
			info.Bias = light.Properties.ShadowBias;
			info.NormalBias = light.Properties.ShadowNormalBias;

			switch (light.Properties.Type) {
			case LightType::Directional:
				info.Type = ShadowType::Directional_CSM;
				info.Priority = 1000.0f;
				info.Bias = m_Config.DirectionalBias;
				break;
			case LightType::Spot:
				info.Type = ShadowType::Spot;
				info.Priority = 100.0f / (glm::distance(cameraPos, light.WorldPosition) + 1.0f);
				info.Bias = m_Config.SpotBias;
				break;
			case LightType::Point:
				info.Type = ShadowType::Point_Cubemap;
				info.Priority = 50.0f / (glm::distance(cameraPos, light.WorldPosition) + 1.0f);
				info.Bias = m_Config.PointBias;
				break;
			default:
				continue;
			}

			m_ShadowCasters.push_back(info);
		}

		// Sort by priority (highest first)
		std::sort(m_ShadowCasters.begin(), m_ShadowCasters.end(),
			[](const ShadowCasterInfo& a, const ShadowCasterInfo& b) {
				return a.Priority > b.Priority;
			});

		// Limit to max shadow-casting lights
		if (m_ShadowCasters.size() > MAX_SHADOW_LIGHTS) {
			m_ShadowCasters.resize(MAX_SHADOW_LIGHTS);
		}
	}

	// ========================================================================
	// STEP 2: ALLOCATE ATLAS LAYERS
	// ========================================================================

	void ShadowSystem::AllocateAtlasLayers() {
		int nextLayer = 0;

		for (auto& caster : m_ShadowCasters) {
			switch (caster.Type) {
			case ShadowType::Directional_CSM:
				caster.LayerCount = m_Config.CSMCascadeCount;
				caster.NumMatrices = m_Config.CSMCascadeCount;
				break;
			case ShadowType::Spot:
				caster.LayerCount = 1;
				caster.NumMatrices = 1;
				break;
			case ShadowType::Point_Cubemap:
				caster.LayerCount = 6;
				caster.NumMatrices = 6;
				break;
			default:
				caster.LayerCount = 0;
				break;
			}

			if (nextLayer + caster.LayerCount > static_cast<int>(m_AtlasMaxLayers)) {
				caster.AtlasFirstLayer = -1;
				continue;
			}

			caster.AtlasFirstLayer = nextLayer;
			for (int j = 0; j < caster.LayerCount; ++j) {
				m_LayerOccupancy[nextLayer + j] = true;
			}
			nextLayer += caster.LayerCount;
		}
	}

	// ========================================================================
	// STEP 3: COMPUTE LIGHT MATRICES
	// ========================================================================

	void ShadowSystem::ComputeLightMatrices(
		const glm::mat4& cameraView,
		const glm::mat4& cameraProj,
		float cameraNear, float cameraFar)
	{
		for (auto& caster : m_ShadowCasters) {
			if (caster.AtlasFirstLayer < 0) continue;

			switch (caster.Type) {
			case ShadowType::Directional_CSM: {
				auto cascades = CascadedShadowMap::CalculateCascades(
					cameraView, cameraProj,
					caster.Direction,
					cameraNear,
					std::min(cameraFar, m_Config.MaxShadowDistance),
					m_Config.CSMCascadeCount,
					m_Config.CSMSplitLambda,
					m_Config.DirectionalResolution
				);
				for (uint32_t c = 0; c < cascades.size() && c < MAX_SHADOW_CASCADES; ++c) {
					caster.ViewProjections[c] = cascades[c].ViewProjection;
					caster.CascadeSplitDepths[c] = cascades[c].SplitDepth;
				}
				break;
			}

			case ShadowType::Spot: {
				float outerAngle = glm::radians(caster.OuterCone);
				float fov = outerAngle * 2.0f;
				fov = glm::clamp(fov, glm::radians(1.0f), glm::radians(179.0f));

				glm::mat4 proj = glm::perspective(fov, 1.0f, 0.1f, caster.Range);
				glm::mat4 view = glm::lookAt(
					caster.Position,
					caster.Position + caster.Direction,
					glm::abs(glm::dot(caster.Direction, glm::vec3(0, 1, 0))) > 0.999f
						? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0)
				);
				caster.ViewProjections[0] = proj * view;
				break;
			}

			case ShadowType::Point_Cubemap: {
				glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, caster.Range);

				struct FaceDir { glm::vec3 target; glm::vec3 up; };
				FaceDir faces[6] = {
					{{ 1, 0, 0}, {0,-1, 0}},
					{{-1, 0, 0}, {0,-1, 0}},
					{{ 0, 1, 0}, {0, 0, 1}},
					{{ 0,-1, 0}, {0, 0,-1}},
					{{ 0, 0, 1}, {0,-1, 0}},
					{{ 0, 0,-1}, {0,-1, 0}},
				};

				for (int f = 0; f < 6; ++f) {
					glm::mat4 view = glm::lookAt(
						caster.Position,
						caster.Position + faces[f].target,
						faces[f].up
					);
					caster.ViewProjections[f] = proj * view;
				}
				break;
			}

			default:
				break;
			}
		}
	}

	// ========================================================================
	// STEP 4: RENDER SHADOW MAPS
	// ========================================================================

	void ShadowSystem::RenderShadowMaps(Scene* scene) {
		LNX_PROFILE_FUNCTION();

		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) return;

		// Save previous state
		int prevViewport[4];
		cmdList->GetViewport(prevViewport);

		// Bind our depth-only FBO
		m_AtlasFBO->Bind();

		// No color output — depth only
		cmdList->SetDrawBuffers({});
		cmdList->SetColorMask(false, false, false, false);
		cmdList->SetDepthMask(true);
		cmdList->SetDepthFunc(RHI::CompareFunc::Less);

		uint64_t fboHandle = m_AtlasFBO->GetNativeHandle();
		uint64_t texHandle = m_AtlasDepthTexture->GetNativeHandle();
		uint32_t res = m_AtlasResolution;

		for (auto& caster : m_ShadowCasters) {
			if (caster.AtlasFirstLayer < 0) continue;

			if (caster.Type == ShadowType::Point_Cubemap) {
				m_DepthPointShader->Bind();

				for (int face = 0; face < 6; ++face) {
					int layer = caster.AtlasFirstLayer + face;

					cmdList->AttachDepthTextureLayer(fboHandle, texHandle, layer);
					cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(res), static_cast<float>(res));
					cmdList->SetClearColor({ 0, 0, 0, 1 });
					cmdList->SetDepthMask(true);
					cmdList->Clear();

					cmdList->SetCullMode(RHI::CullMode::None);
					cmdList->SetPolygonOffset(true, caster.Bias * 4.0f, caster.Bias * 4.0f);

					m_DepthUBOData.LightVP = caster.ViewProjections[face];
					m_DepthUBOData.LightPosAndRange = glm::vec4(caster.Position, caster.Range);
					RenderSceneDepthPoint(scene, caster.Position, caster.Range, caster.ViewProjections);

					cmdList->SetPolygonOffset(false);
					m_Stats.PointFacesRendered++;
				}
				m_Stats.ShadowMapsRendered++;
			}
			else {
				m_DepthShader->Bind();
				cmdList->SetCullMode(RHI::CullMode::None);

				for (int m = 0; m < caster.NumMatrices; ++m) {
					int layer = caster.AtlasFirstLayer + m;

					cmdList->AttachDepthTextureLayer(fboHandle, texHandle, layer);
					cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(res), static_cast<float>(res));
					cmdList->SetClearColor({ 0, 0, 0, 1 });
					cmdList->SetDepthMask(true);
					cmdList->Clear();

					cmdList->SetPolygonOffset(true, caster.Bias * 2.0f, caster.Bias * 2.0f);

					m_DepthUBOData.LightVP = caster.ViewProjections[m];
					RenderSceneDepthOnly(scene, caster.ViewProjections[m]);

					cmdList->SetPolygonOffset(false);

					if (caster.Type == ShadowType::Directional_CSM) {
						m_Stats.CascadesRendered++;
					} else {
						m_Stats.SpotMapsRendered++;
					}
				}
				m_Stats.ShadowMapsRendered++;
			}
		}

		// Restore state
		cmdList->SetColorMask(true, true, true, true);
		cmdList->SetCullMode(RHI::CullMode::Back);

		m_AtlasFBO->Unbind();
		cmdList->SetViewport(
			static_cast<float>(prevViewport[0]), static_cast<float>(prevViewport[1]),
			static_cast<float>(prevViewport[2]), static_cast<float>(prevViewport[3])
		);
	}

	// ========================================================================
	// RENDER SCENE DEPTH ONLY (shared by directional & spot)
	// ========================================================================

	void ShadowSystem::RenderSceneDepthOnly(Scene* scene, const glm::mat4& lightVP) {
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.MeshModel) continue;

			glm::mat4 worldTransform = scene->GetWorldTransform(entity);

			m_DepthUBOData.Model = worldTransform;
			m_DepthShaderUBO->SetData(&m_DepthUBOData, sizeof(DepthShaderUBOData));

			for (const auto& submesh : mesh.MeshModel->GetMeshes()) {
				auto va = submesh->GetVertexArray();
				if (va) {
					va->Bind();
					auto* cmdList = RHI::GetImmediateCommandList();
					cmdList->DrawIndexed(static_cast<uint32_t>(submesh->GetIndices().size()));
					m_Stats.ShadowDrawCalls++;
				}
			}
		}
	}

	void ShadowSystem::RenderSceneDepthPoint(Scene* scene, const glm::vec3& lightPos,
											  float lightRange, const glm::mat4 faceVPs[6])
	{
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.MeshModel) continue;

			glm::mat4 worldTransform = scene->GetWorldTransform(entity);

			m_DepthUBOData.Model = worldTransform;
			m_DepthShaderUBO->SetData(&m_DepthUBOData, sizeof(DepthShaderUBOData));

			for (const auto& submesh : mesh.MeshModel->GetMeshes()) {
				auto va = submesh->GetVertexArray();
				if (va) {
					va->Bind();
					auto* cmdList = RHI::GetImmediateCommandList();
					cmdList->DrawIndexed(static_cast<uint32_t>(submesh->GetIndices().size()));
					m_Stats.ShadowDrawCalls++;
				}
			}
		}
	}

	// ========================================================================
	// STEP 5: UPLOAD GPU DATA
	// ========================================================================

	void ShadowSystem::UploadGPUData() {
		memset(&m_GPUData, 0, sizeof(m_GPUData));

		m_GPUData.NumShadowLights = static_cast<int>(m_ShadowCasters.size());
		m_GPUData.CSMCascadeCount = m_Config.CSMCascadeCount;
		m_GPUData.MaxShadowDistance = m_Config.MaxShadowDistance;
		m_GPUData.DistanceSofteningStart = m_Config.DistanceSofteningStart;
		m_GPUData.DistanceSofteningMax = m_Config.DistanceSofteningMax;
		m_GPUData.SkyTintStrength = m_Config.EnableSkyColorTint ? m_Config.SkyTintStrength : 0.0f;

		for (const auto& caster : m_ShadowCasters) {
			if (caster.Type == ShadowType::Directional_CSM && caster.AtlasFirstLayer >= 0) {
				for (uint32_t c = 0; c < m_Config.CSMCascadeCount && c < MAX_SHADOW_CASCADES; ++c) {
					m_GPUData.Cascades[c].ViewProjection = caster.ViewProjections[c];
					m_GPUData.Cascades[c].SplitDepth = caster.CascadeSplitDepths[c];
				}
				break;
			}
		}

		int shadowIdx = 0;
		for (const auto& caster : m_ShadowCasters) {
			if (shadowIdx >= MAX_SHADOW_LIGHTS) break;
			if (caster.AtlasFirstLayer < 0) continue;

			auto& sd = m_GPUData.Shadows[shadowIdx];

			for (int m = 0; m < caster.NumMatrices && m < 6; ++m) {
				sd.ViewProjection[m] = caster.ViewProjections[m];
			}

			sd.ShadowParams = glm::vec4(
				caster.Bias,
				caster.NormalBias,
				static_cast<float>(caster.AtlasFirstLayer),
				static_cast<float>(caster.Type)
			);

			sd.ShadowParams2 = glm::vec4(
				caster.Range,
				m_Config.PCFRadius,
				static_cast<float>(caster.NumMatrices),
				static_cast<float>(m_AtlasResolution)
			);

			shadowIdx++;
		}

		m_ShadowUBO->SetData(&m_GPUData, sizeof(ShadowUniformData));
	}
} // namespace Lunex