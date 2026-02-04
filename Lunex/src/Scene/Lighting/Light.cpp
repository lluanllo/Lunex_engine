#include "stpch.h"
#include "Light.h"

#include <glm/gtc/constants.hpp>

namespace Lunex {

	Light::Light()
		: m_Properties()
	{
	}

	Light::Light(LightType type)
		: m_Properties()
	{
		m_Properties.Type = type;
		
		// Auto-enable sun settings for directional lights
		if (type == LightType::Directional) {
			m_Properties.SunSky.IsSunLight = true;
			m_Properties.SunSky.LinkToSkyboxRotation = true;
		}
	}

	float Light::CalculateSkyboxRotationFromDirection(const glm::vec3& direction) {
		// The light direction points from the light source.
		// For a directional light, this is typically the forward vector (-Z in local space)
		// transformed to world space.
		// 
		// To find the skybox rotation, we need the azimuth angle of the sun position.
		// The sun position is opposite to the light direction (the direction TO the sun).
		glm::vec3 toSun = -glm::normalize(direction);
		
		// Calculate azimuth (rotation around Y axis)
		// atan2 returns angle in radians from -PI to PI
		float azimuth = glm::atan(toSun.x, toSun.z);
		
		// Convert to degrees
		return glm::degrees(azimuth);
	}

	float Light::CalculateSunElevation(const glm::vec3& direction) {
		// The elevation is the angle above/below the horizon
		// direction.y gives us sin(elevation) since direction is normalized
		glm::vec3 toSun = -glm::normalize(direction);
		
		// asin returns angle in radians from -PI/2 to PI/2
		float elevation = glm::asin(glm::clamp(toSun.y, -1.0f, 1.0f));
		
		return glm::degrees(elevation);
	}

	float Light::CalculateSunAzimuth(const glm::vec3& direction) {
		// Calculate azimuth (compass direction: N=0, E=90, S=180, W=270)
		glm::vec3 toSun = -glm::normalize(direction);
		
		float azimuth = glm::atan(toSun.x, toSun.z);
		
		// Convert to 0-360 range
		float azimuthDeg = glm::degrees(azimuth);
		if (azimuthDeg < 0.0f) {
			azimuthDeg += 360.0f;
		}
		
		return azimuthDeg;
	}

} // namespace Lunex
