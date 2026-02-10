#include "stpch.h"
#include "ShadowSystem.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/Lighting/LightSystem.h"
#include "Scene/Camera/EditorCamera.h"
#include "Resources/Mesh/Model.h"
#include "Log/Log.h"

#include <glad/glad.h>
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
		// 4 cascades + 16 lights * 6 faces = 100 layers worst case
		// In practice we cap at a reasonable number.
		m_AtlasMaxLayers = MAX_SHADOW_CASCADES + MAX_SHADOW_LIGHTS * 6;
		m_LayerOccupancy.resize(m_AtlasMaxLayers, false);

		// ---- Create depth texture array (atlas) ----
		// All layers use the same resolution (the max of all types)
		m_AtlasResolution = std::max({ m_Config.DirectionalResolution,
								  m_Config.SpotResolution,
								  m_Config.PointResolution });

		glGenTextures(1, &m_AtlasDepthTexture);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_AtlasDepthTexture);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F,
					   m_AtlasResolution, m_AtlasResolution, m_AtlasMaxLayers);

		// Shadow comparison sampler state
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

		// ---- Create FBO for shadow rendering ----
		glGenFramebuffers(1, &m_AtlasFBO);

		// ---- Create UBO (binding = 7, avoids conflict with Skybox at binding 4) ----
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

		if (m_AtlasFBO) {
			glDeleteFramebuffers(1, &m_AtlasFBO);
			m_AtlasFBO = 0;
		}
		if (m_AtlasDepthTexture) {
			glDeleteTextures(1, &m_AtlasDepthTexture);
			m_AtlasDepthTexture = 0;
		}

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

		// Bind shadow atlas to texture slot 11 (comparison sampler)
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_AtlasDepthTexture);

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
				info.Priority = 1000.0f; // Always highest priority
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
				// No room — skip this light
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

				// 6 cubemap face directions (same order as GL_TEXTURE_CUBE_MAP_POSITIVE_X, etc.)
				struct FaceDir { glm::vec3 target; glm::vec3 up; };
				FaceDir faces[6] = {
					{{ 1, 0, 0}, {0,-1, 0}},  // +X
					{{-1, 0, 0}, {0,-1, 0}},  // -X
					{{ 0, 1, 0}, {0, 0, 1}},  // +Y
					{{ 0,-1, 0}, {0, 0,-1}},  // -Y
					{{ 0, 0, 1}, {0,-1, 0}},  // +Z
					{{ 0, 0,-1}, {0,-1, 0}},  // -Z
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

		// Save GL state
		GLint prevFBO = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
		GLint prevViewport[4];
		glGetIntegerv(GL_VIEWPORT, prevViewport);
		GLboolean prevCullFace = glIsEnabled(GL_CULL_FACE);
		GLint prevCullMode;
		glGetIntegerv(GL_CULL_FACE_MODE, &prevCullMode);

		// Bind our FBO
		glBindFramebuffer(GL_FRAMEBUFFER, m_AtlasFBO);

		// Enable depth writing, disable color
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		// All layers use the full atlas resolution
		uint32_t res = m_AtlasResolution;

		for (auto& caster : m_ShadowCasters) {
			if (caster.AtlasFirstLayer < 0) continue;

			if (caster.Type == ShadowType::Point_Cubemap) {
				// Point light: render 6 faces with linear depth shader
				m_DepthPointShader->Bind();

				for (int face = 0; face < 6; ++face) {
					int layer = caster.AtlasFirstLayer + face;

					glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
											  m_AtlasDepthTexture, 0, layer);
					glViewport(0, 0, res, res);
					glClear(GL_DEPTH_BUFFER_BIT);

					// Disable face culling for point shadows to capture all geometry
					glDisable(GL_CULL_FACE);

					// Polygon offset for bias
					glEnable(GL_POLYGON_OFFSET_FILL);
					glPolygonOffset(caster.Bias * 4.0f, caster.Bias * 4.0f);

					// Set UBO data for this face
					m_DepthUBOData.LightVP = caster.ViewProjections[face];
					m_DepthUBOData.LightPosAndRange = glm::vec4(caster.Position, caster.Range);
					RenderSceneDepthPoint(scene, caster.Position, caster.Range, caster.ViewProjections);

					glDisable(GL_POLYGON_OFFSET_FILL);
					m_Stats.PointFacesRendered++;
				}
				m_Stats.ShadowMapsRendered++;
			}
			else {
				// Directional / Spot: standard depth shader
				m_DepthShader->Bind();

				// Disable face culling to capture all geometry (thin planes, single-sided meshes)
				glDisable(GL_CULL_FACE);

				for (int m = 0; m < caster.NumMatrices; ++m) {
					int layer = caster.AtlasFirstLayer + m;

					glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
											  m_AtlasDepthTexture, 0, layer);
					glViewport(0, 0, res, res);
					glClear(GL_DEPTH_BUFFER_BIT);

					// Polygon offset bias
					glEnable(GL_POLYGON_OFFSET_FILL);
					glPolygonOffset(caster.Bias * 2.0f, caster.Bias * 2.0f);

					m_DepthUBOData.LightVP = caster.ViewProjections[m];
					RenderSceneDepthOnly(scene, caster.ViewProjections[m]);

					glDisable(GL_POLYGON_OFFSET_FILL);

					if (caster.Type == ShadowType::Directional_CSM) {
						m_Stats.CascadesRendered++;
					} else {
						m_Stats.SpotMapsRendered++;
					}
				}
				m_Stats.ShadowMapsRendered++;
			}
		}

		// Restore GL state
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		if (prevCullFace) {
			glEnable(GL_CULL_FACE);
		} else {
			glDisable(GL_CULL_FACE);
		}
		glCullFace(prevCullMode);

		glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
		glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
	}

	// ========================================================================
	// RENDER SCENE DEPTH ONLY (shared by directional & spot)
	// ========================================================================

	void ShadowSystem::RenderSceneDepthOnly(Scene* scene, const glm::mat4& lightVP) {
		// Iterate all mesh entities and draw with depth-only shader
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.MeshModel) continue;

			glm::mat4 worldTransform = scene->GetWorldTransform(entity);

			// Upload per-object UBO data
			m_DepthUBOData.Model = worldTransform;
			m_DepthShaderUBO->SetData(&m_DepthUBOData, sizeof(DepthShaderUBOData));

			// Draw all submeshes (depth only — no material)
			for (const auto& submesh : mesh.MeshModel->GetMeshes()) {
				auto va = submesh->GetVertexArray();
				if (va) {
					va->Bind();
					glDrawElements(GL_TRIANGLES,
								   static_cast<GLsizei>(submesh->GetIndices().size()),
								   GL_UNSIGNED_INT, nullptr);
					m_Stats.ShadowDrawCalls++;
				}
			}
		}
	}

	void ShadowSystem::RenderSceneDepthPoint(Scene* scene, const glm::vec3& lightPos,
											  float lightRange, const glm::mat4 faceVPs[6])
	{
		// For point lights — same as depth only but with the point shader bound
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.MeshModel) continue;

			glm::mat4 worldTransform = scene->GetWorldTransform(entity);

			// Upload per-object UBO data
			m_DepthUBOData.Model = worldTransform;
			m_DepthShaderUBO->SetData(&m_DepthUBOData, sizeof(DepthShaderUBOData));

			for (const auto& submesh : mesh.MeshModel->GetMeshes()) {
				auto va = submesh->GetVertexArray();
				if (va) {
					va->Bind();
					glDrawElements(GL_TRIANGLES,
								   static_cast<GLsizei>(submesh->GetIndices().size()),
								   GL_UNSIGNED_INT, nullptr);
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

		// Fill cascade data from first directional light
		for (const auto& caster : m_ShadowCasters) {
			if (caster.Type == ShadowType::Directional_CSM && caster.AtlasFirstLayer >= 0) {
				// We already have the VPs in caster.ViewProjections
				// We just need split depths — recalculate quickly
				float nearDist = 0.1f;
				float farDist = m_Config.MaxShadowDistance;
				for (uint32_t c = 0; c < m_Config.CSMCascadeCount && c < MAX_SHADOW_CASCADES; ++c) {
					m_GPUData.Cascades[c].ViewProjection = caster.ViewProjections[c];

					float p = static_cast<float>(c + 1) / static_cast<float>(m_Config.CSMCascadeCount);
					float logSplit = nearDist * std::pow(farDist / nearDist, p);
					float uniformSplit = nearDist + (farDist - nearDist) * p;
					m_GPUData.Cascades[c].SplitDepth = m_Config.CSMSplitLambda * logSplit +
														(1.0f - m_Config.CSMSplitLambda) * uniformSplit;
				}
				break; // Only first directional gets CSM cascade data
			}
		}

		// Fill per-light shadow data
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

			// All layers use the same atlas resolution
			sd.ShadowParams2 = glm::vec4(
				caster.Range,
				m_Config.PCFRadius,
				static_cast<float>(caster.NumMatrices),
				static_cast<float>(m_AtlasResolution)
			);

			shadowIdx++;
		}

		// Upload
		m_ShadowUBO->SetData(&m_GPUData, sizeof(ShadowUniformData));
	}
} // namespace Lunex