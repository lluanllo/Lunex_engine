#pragma once

/**
 * @file Light.h
 * @brief Light class for scene entities
 * 
 * AAA Architecture: Light lives in Scene/Lighting/
 * This is the high-level light used by LightComponent.
 */

#include "LightTypes.h"
#include "Core/Core.h"

namespace Lunex {

	/**
	 * @class Light
	 * @brief Light object for scene entities
	 */
	class Light {
	public:
		Light();
		Light(LightType type);
		~Light() = default;

		// Type
		void SetType(LightType type) { m_Properties.Type = type; }
		LightType GetType() const { return m_Properties.Type; }

		// Color and intensity
		void SetColor(const glm::vec3& color) { m_Properties.Color = color; }
		void SetIntensity(float intensity) { m_Properties.Intensity = glm::max(0.0f, intensity); }
		
		const glm::vec3& GetColor() const { return m_Properties.Color; }
		float GetIntensity() const { return m_Properties.Intensity; }

		// Range (Point & Spot)
		void SetRange(float range) { m_Properties.Range = glm::max(0.0f, range); }
		void SetAttenuation(const glm::vec3& attenuation) { m_Properties.Attenuation = attenuation; }
		
		float GetRange() const { return m_Properties.Range; }
		const glm::vec3& GetAttenuation() const { return m_Properties.Attenuation; }

		// Cone angles (Spot only)
		void SetInnerConeAngle(float angle) { m_Properties.InnerConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		void SetOuterConeAngle(float angle) { m_Properties.OuterConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		
		float GetInnerConeAngle() const { return m_Properties.InnerConeAngle; }
		float GetOuterConeAngle() const { return m_Properties.OuterConeAngle; }

		// Shadows
		void SetCastShadows(bool cast) { m_Properties.CastShadows = cast; }
		bool GetCastShadows() const { return m_Properties.CastShadows; }

		// Get full properties
		const LightProperties& GetProperties() const { return m_Properties; }
		LightProperties& GetProperties() { return m_Properties; }

		// Get GPU-ready data
		LightData GetLightData(const glm::vec3& position, const glm::vec3& direction) const {
			return m_Properties.ToGPUData(position, direction);
		}

	private:
		LightProperties m_Properties;
	};

} // namespace Lunex
