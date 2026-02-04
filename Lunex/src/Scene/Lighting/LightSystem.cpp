#include "stpch.h"
#include "LightSystem.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/SkyboxRenderer.h"
#include "Log/Log.h"

namespace Lunex {

	LightSystem& LightSystem::Get() {
		static LightSystem instance;
		return instance;
	}

	void LightSystem::Initialize() {
		if (m_Initialized) return;
		
		m_Lights.clear();
		m_AmbientColor = glm::vec3(0.03f);
		m_AmbientIntensity = 1.0f;
		m_DirectionalCount = 0;
		m_PointCount = 0;
		m_SpotCount = 0;
		
		// Reset sun light state
		m_HasSunLight = false;
		m_SunLightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
		m_SunLightColor = glm::vec3(1.0f);
		m_SunLightIntensity = 1.0f;
		m_SunLightIntensityMultiplier = 1.0f;
		
		m_Initialized = true;
		LNX_LOG_INFO("LightSystem initialized");
	}

	void LightSystem::Shutdown() {
		m_Lights.clear();
		m_Initialized = false;
		LNX_LOG_INFO("LightSystem shutdown");
	}

	void LightSystem::SyncFromScene(Scene* scene) {
		if (!scene) return;
		
		m_Lights.clear();
		
		// Reset sun light tracking
		m_HasSunLight = false;
		bool foundSunLight = false;
		
		auto view = scene->GetAllEntitiesWith<LightComponent, TransformComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			
			auto& lightComp = entity.GetComponent<LightComponent>();
			auto& transform = entity.GetComponent<TransformComponent>();
			
			if (!lightComp.LightInstance) continue;
			
			LightEntry entry;
			entry.EntityID = entity.GetUUID();
			entry.WorldPosition = transform.Translation;
			
			// Calculate world direction from rotation
			glm::mat4 rotationMatrix = glm::toMat4(glm::quat(transform.Rotation));
			entry.WorldDirection = -glm::normalize(glm::vec3(rotationMatrix[2]));
			
			// Copy properties from Light component
			auto* light = lightComp.LightInstance.get();
			entry.Properties.Type = light->GetType();
			entry.Properties.Color = light->GetColor();
			entry.Properties.Intensity = light->GetIntensity();
			entry.Properties.Range = light->GetRange();
			entry.Properties.Attenuation = light->GetAttenuation();
			entry.Properties.InnerConeAngle = light->GetInnerConeAngle();
			entry.Properties.OuterConeAngle = light->GetOuterConeAngle();
			entry.Properties.CastShadows = light->GetCastShadows();
			
			// Copy Sun/Sky settings
			entry.Properties.SunSky = light->GetSunSkySettings();
			
			entry.IsActive = true;
			entry.IsVisible = true;
			
			m_Lights.push_back(entry);
			
			// Check if this is the Sun Light (only first one found)
			if (!foundSunLight && 
				entry.Properties.Type == LightType::Directional && 
				entry.Properties.SunSky.IsSunLight) {
				
				foundSunLight = true;
				m_HasSunLight = true;
				m_SunLightDirection = entry.WorldDirection;
				m_SunLightColor = entry.Properties.Color;
				m_SunLightIntensity = entry.Properties.Intensity;
				m_SunLightIntensityMultiplier = entry.Properties.SunSky.SkyboxIntensityMultiplier;
				
				// Sync with SkyboxRenderer if link is enabled
				if (entry.Properties.SunSky.LinkToSkyboxRotation) {
					SkyboxRenderer::SetSyncWithSunLight(true);
					SkyboxRenderer::UpdateSunLightDirection(entry.WorldDirection);
					SkyboxRenderer::SetSunLightIntensityMultiplier(entry.Properties.SunSky.SkyboxIntensityMultiplier);
				} else {
					SkyboxRenderer::SetSyncWithSunLight(false);
				}
			}
		}
		
		// If no sun light found, disable sync
		if (!foundSunLight) {
			SkyboxRenderer::SetSyncWithSunLight(false);
		}
		
		UpdateLightCounts();
	}

	void LightSystem::ClearLights() {
		m_Lights.clear();
		m_DirectionalCount = 0;
		m_PointCount = 0;
		m_SpotCount = 0;
		
		m_HasSunLight = false;
		SkyboxRenderer::SetSyncWithSunLight(false);
	}

	void LightSystem::CullLights(const ViewFrustum& frustum) {
		for (auto& light : m_Lights) {
			light.IsVisible = IsLightVisible(light, frustum);
		}
	}

	LightingData LightSystem::GetLightingData() const {
		LightingData data;
		data.AmbientColor = m_AmbientColor;
		data.AmbientIntensity = m_AmbientIntensity;
		data.DirectionalLightCount = 0;
		data.PointLightCount = 0;
		data.SpotLightCount = 0;
		
		// Sun light data
		data.HasSunLight = m_HasSunLight;
		data.SunDirection = m_SunLightDirection;
		data.SunColor = m_SunLightColor;
		data.SunIntensity = m_SunLightIntensity;
		
		data.Lights.reserve(m_Lights.size());
		
		for (const auto& light : m_Lights) {
			if (!light.IsActive || !light.IsVisible) continue;
			
			LightData gpuData = light.Properties.ToGPUData(light.WorldPosition, light.WorldDirection);
			data.Lights.push_back(gpuData);
			
			switch (light.Properties.Type) {
				case LightType::Directional:
					data.DirectionalLightCount++;
					break;
				case LightType::Point:
					data.PointLightCount++;
					break;
				case LightType::Spot:
					data.SpotLightCount++;
					break;
				default:
					break;
			}
		}
		
		return data;
	}

	uint32_t LightSystem::GetVisibleLightCount() const {
		uint32_t count = 0;
		for (const auto& light : m_Lights) {
			if (light.IsActive && light.IsVisible) {
				count++;
			}
		}
		return count;
	}

	void LightSystem::UpdateLightCounts() {
		m_DirectionalCount = 0;
		m_PointCount = 0;
		m_SpotCount = 0;
		
		for (const auto& light : m_Lights) {
			switch (light.Properties.Type) {
				case LightType::Directional:
					m_DirectionalCount++;
					break;
				case LightType::Point:
					m_PointCount++;
					break;
				case LightType::Spot:
					m_SpotCount++;
					break;
				default:
					break;
			}
		}
	}

	bool LightSystem::IsLightVisible(const LightEntry& light, const ViewFrustum& frustum) const {
		// Directional lights are always visible
		if (light.Properties.Type == LightType::Directional) {
			return true;
		}
		
		// Point and spot lights: check sphere against frustum
		float radius = light.Properties.Range;
		return frustum.IntersectsSphere(light.WorldPosition, radius);
	}

} // namespace Lunex
